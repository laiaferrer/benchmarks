#include <chrono>
#include <iostream>
#include <thread>
#include <utility>
#include "libnvme.h"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/nvme_ioctl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <vector>
#include <iomanip>
#include <random>

typedef enum {
    KV_OPC_STORE = 0x01,
    KV_OPC_RETRIEVE = 0x02,
    KV_OPC_LIST = 0x06,
    KV_OPC_DELETE = 0x10,
    KV_OPC_EXISTS = 0x14,
} kv_opcode_e;

const size_t BUFFER_SIZE = 4096;
int ret = 0;
__u8 opcode = 0x14; 
__u8 flags = 0;
__u16 rsvd = 0;
__u32 nsid = 1;
__u32 cdw2 = 0;
__u32 cdw3 = 0;
__u32 cdw10 = 0;
__u32 cdw11 = 0;
__u32 cdw12 = 0;
__u32 cdw13 = 0;
__u32 cdw14 = 0;
__u32 cdw15 = 0;
__u32 data_len = 0;
void *data = NULL;
__u32 metadata_len = 0;
void *metadata = NULL;
__u32 timeout_ms = 1000;
__u32 result;

int open_nvme_device() {
    int fd = open("/dev/ng0n1", O_RDWR);
    if (fd < 0) {
        perror("Error opening the NVMe device");
    }
    return fd;
}

void close_nvme_device(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

std::vector<uint32_t> key_creation(int n) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

    std::vector<uint32_t> cdw2_vector(n);

    for (size_t i = 0; i < n; ++i) {
        cdw2_vector[i] = dist(gen);
    }

    return cdw2_vector;
}

std::vector<char> generate_random_chars(size_t length) {
    std::vector<char> random_chars(length);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<char> dis('!', '~'); // Random chars from printable ASCII range

    for (auto& ch : random_chars) {
        ch = dis(gen);
    }

    return random_chars;
}

void retrieve (int n, int type, std::vector<uint32_t> cdw2_vector) {
    int fd = open_nvme_device();
    auto random_chars = generate_random_chars(4096);
    char* kitty = random_chars.data();
    char c = 'k';
    nsid = 1;
    data = kitty;
    opcode = KV_OPC_RETRIEVE;
    data_len = 4096;
    cdw11 = 4;                           //key size
    cdw10 = 4096; 
    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; ++i) {
        ret = nvme_io_passthru(fd, opcode, flags, rsvd, nsid, cdw2_vector[i], cdw3,
                             cdw10, cdw11, cdw12, cdw13, cdw14, cdw15, data_len,
                             data, metadata_len, metadata, timeout_ms, &result);
    }
     auto end_time = std::chrono::high_resolution_clock::now();
     std::chrono::duration<double> elapsed = end_time - start_time;
     std::cout << "--TIME RETRIEVE " << type << ": " << elapsed.count() << " segundos" << std::endl;
     std::cout << "TIME OF ONE RETRIEVE: " << elapsed.count() / n << " segundos" << std::endl;
     std::cout << "RETRIEVE PER SECOND: " << 60 / (elapsed.count() / n) << " segundos" << std::endl;
}

void store (int n, int type, std::vector<uint32_t> cdw2_vector) {
    int fd = open_nvme_device();
    auto random_chars = generate_random_chars(4096);
    char* kitty = random_chars.data();
    char c = 'k';
    nsid = 1;
    data = kitty;
    opcode = KV_OPC_STORE;
    data_len = 4096;
    cdw11 = 4;                           //key size
    cdw10 = 4096; 
    cdw2 = 0xcccccc3c;                 //key value
    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < n; ++i) {
        ret = nvme_io_passthru(fd, opcode, flags, rsvd, nsid, cdw2_vector[i], cdw3,
                             cdw10, cdw11, cdw12, cdw13, cdw14, cdw15, data_len,
                             data, metadata_len, metadata, timeout_ms, &result);
    }
     auto end_time = std::chrono::high_resolution_clock::now();
     std::chrono::duration<double> elapsed = end_time - start_time;
     std::cout << "--TIME STORE " << type << ": " << elapsed.count() << " segundos" << std::endl;
     std::cout << "TIME OF ONE STORE: " << elapsed.count() / n << " segundos" << std::endl;
      std::cout << "STORE PER SECOND: " << 60 / (elapsed.count() / n) << " segundos" << std::endl;
}

void thread_creation(int num_threads, int iterations) {

    std::vector<std::thread> store_threads;
    std::vector<std::thread> retrieve_threads;
    std::vector<std::vector<uint32_t>> all_vectors;

    for (int i = 1; i <= num_threads; ++i) {
        std::vector<uint32_t> cdw2_vector(iterations);
        cdw2_vector = key_creation(iterations);
        store_threads.emplace_back(store, iterations, i, cdw2_vector);
        all_vectors.push_back(cdw2_vector);
    }

    for (int i = 0; i < num_threads; ++i) {
        store_threads[i].join();
    }

    for (int i = 1; i <= num_threads; ++i) {
        retrieve_threads.emplace_back(retrieve, iterations, i, std::ref(all_vectors[i-1]));
    }

    for (int i = 0; i < num_threads; ++i) {
        retrieve_threads[i].join();
    }
}

int main() {
    //store(1, 1, std::vector<uint32_t>({0xdeadbeef}));
    //retrieve(100);
    auto start_time = std::chrono::high_resolution_clock::now();
    thread_creation(4, 100);
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << "TOTAL TIME: " << elapsed.count() << " segundos" << std::endl;

    return 0;
}