// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/userspace_swap/swap_storage.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <chrono>
#include <random>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/types/fixed_array.h"
#include "chromeos/ash/components/memory/userspace_swap/region.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace memory {
namespace userspace_swap {

class SwapStorageTest : public testing::Test,
                        public testing::WithParamInterface<SwapFile::Type> {
 public:
  SwapStorageTest()
      : random_(/* seen prng */
                std::chrono::system_clock::now().time_since_epoch().count()) {}

 protected:
  void SetUp() override {
    // Start by creating a temporary file we will use for this test and then
    // wrapping it in the swap type we're testing.
    base::FilePath temp_file;
    ASSERT_TRUE(base::CreateTemporaryFile(&temp_file));

    base::ScopedFD swap_fd;
    swap_fd.reset(HANDLE_EINTR(open(temp_file.MaybeAsASCII().c_str(),
                                    O_RDWR | O_CLOEXEC, S_IRUSR | S_IWUSR)));

    ASSERT_NE(unlink(temp_file.MaybeAsASCII().c_str()), -1);

    ASSERT_TRUE(swap_fd.is_valid());

    // Wrap this FD in the swap file type for this test.
    swap_ = SwapFile::WrapFD(std::move(swap_fd), GetParam());
    ASSERT_NE(swap_, nullptr);
  }

  void FillRandom(std::string* str, int size) {
    for (int j = 0; j < size; ++j) {
      str->append(1, static_cast<char>(random_() % 255));
    }

    ASSERT_EQ(str->size(), static_cast<std::string::size_type>(size));
  }

  std::default_random_engine random_;
  std::unique_ptr<SwapFile> swap_;
};

// We test all the different variations of a swap file to make sure all
// functionality is properly implemented, this will test:
// - Standard swap file with no compression or encryption.
// - A compressed swap file.
// - An encrypted swap file.
// - A compressed and encrypted swap file.
INSTANTIATE_TEST_SUITE_P(
    SwapStorageTest,
    SwapStorageTest,
    testing::Values(
        SwapFile::Type::kStandard /* no compression or encryption */,
        SwapFile::Type::kCompressed,
        SwapFile::Type::kEncrypted,
        SwapFile::Type::kEncrypted | SwapFile::Type::kCompressed));

TEST_P(SwapStorageTest, SimpleWriteRead) {
  std::string buffer = "hello world";
  constexpr size_t buffer_len = sizeof("hello world") - 1;

  // Swap address can be at index 0 so we use UINT64_MAX to differentiate.
  Region swap_region(std::numeric_limits<uint64_t>::max(), 0);

  // Write it to swap and validate we also got back sane values for swap pos and
  // length.
  ASSERT_TRUE(
      swap_->WriteToSwap(Region(buffer.c_str(), buffer_len), &swap_region));
  ASSERT_NE(swap_region.address, std::numeric_limits<uint64_t>::max());
  ASSERT_NE(swap_region.length, 0u);

  // Read the region from swap in [swap_pos, swap_pos + swap_len]
  char read_buf[buffer_len];
  memset(read_buf, 0, sizeof(read_buf));
  ASSERT_EQ(
      swap_->ReadFromSwap(swap_region, Region(read_buf, sizeof(read_buf))),
      static_cast<ssize_t>(sizeof(read_buf)));

  // We should have correctly read back what we wrote.
  ASSERT_EQ(memcmp(read_buf, buffer.c_str(), sizeof(read_buf)), 0);
}

TEST_P(SwapStorageTest, ManyWriteRead) {
  // Write 1000 random length buffers and then read them back in a random order
  // and make sure they are as expected.
  std::vector<std::pair<std::string, Region>> buffers;

  constexpr int kNumBuffers = 1000;
  buffers.reserve(kNumBuffers);
  for (int i = 0; i < kNumBuffers; ++i) {
    // Choose a random length between 1 byte and 10KB
    int buffer_len = (random_() % (10 << 10)) + 1;
    std::string buf;
    buf.reserve(buffer_len);
    FillRandom(&buf, buffer_len);

    // Swap pos can be at index 0 so we use UINT64_MAX to differentiate.
    Region swap_region(std::numeric_limits<uint64_t>::max(), 0);

    // Write it to swap and validate we also got back sane values for swap pos
    // and length.
    ASSERT_TRUE(
        swap_->WriteToSwap(Region(buf.c_str(), buf.size()), &swap_region));
    ASSERT_NE(swap_region.address, std::numeric_limits<uint64_t>::max());
    ASSERT_NE(swap_region.length, 0u);

    // Save where this buffer was written.
    buffers.emplace_back(std::move(buf), swap_region);
  }

  // Shuffle the ordering of the buffers so we read them back in a random order.
  std::shuffle(buffers.begin(), buffers.end(), random_);

  // Read back all the regions and verify.
  for (const auto& buf : buffers) {
    base::FixedArray<char> read_buf(buf.first.size());
    ASSERT_EQ(swap_->ReadFromSwap(/* Region */ buf.second,
                                  Region(read_buf.data(), read_buf.memsize())),
              static_cast<ssize_t>(read_buf.memsize()));

    // We should have correctly read back what we wrote.
    ASSERT_EQ(memcmp(read_buf.data(), buf.first.c_str(), read_buf.memsize()),
              0);

    // Now drop it from the swap.
    ASSERT_TRUE(swap_->DropFromSwap(/* Region */ buf.second));
  }
}

TEST_P(SwapStorageTest, DropFromSwap) {
  // This test validates that we can drop what we wrote from the swap file and
  // the block size will return to the original size.
  uint64_t block_size_kb_before = swap_->GetUsageKB();
  std::string buffer;

  // We want to fill buffer with what should be many blocks of random data so we
  // can fully observe the growing and shrinking size.
  FillRandom(&buffer, 32 * (4 << 10));

  // Swap pos can be at index 0 so we use UINT64_MAX to differentiate.
  Region swap_region(std::numeric_limits<uint64_t>::max(), 0);

  // Write it to swap and validate we also got back sane values for swap pos and
  // length.
  ASSERT_TRUE(
      swap_->WriteToSwap(Region(buffer.c_str(), buffer.size()), &swap_region));
  ASSERT_NE(swap_region.address, std::numeric_limits<uint64_t>::max());
  ASSERT_NE(swap_region.length, 0u);

  uint64_t block_size_kb = swap_->GetUsageKB();
  ASSERT_GT(block_size_kb, block_size_kb_before);

  // Read the region from swap in [swap_pos, swap_pos + swap_len]
  base::FixedArray<char> read_buf(buffer.length());
  ASSERT_EQ(swap_->ReadFromSwap(swap_region,
                                Region(read_buf.data(), read_buf.memsize())),
            static_cast<ssize_t>(read_buf.memsize()));

  // We should have correctly read back what we wrote.
  ASSERT_EQ(memcmp(read_buf.data(), buffer.c_str(), read_buf.memsize()), 0);

  // Now we will drop it.
  ASSERT_TRUE(swap_->DropFromSwap(swap_region));

  // Finally check the size.
  uint64_t block_size_kb_end = swap_->GetUsageKB();
  ASSERT_LT(block_size_kb_end, block_size_kb);
}

}  // namespace userspace_swap
}  // namespace memory
}  // namespace ash
