// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/memory/pagemap.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <random>

#include "base/files/scoped_file.h"
#include "base/memory/page_size.h"
#include "base/posix/eintr_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace memory {

namespace {
template <typename T, size_t N>
constexpr size_t countof(T (&array)[N]) {
  return N;
}

}  // namespace

class PagemapTest : public testing::Test {
 public:
  void SetUp() override {
    // Use a memfd so we can truncate it to arbitrary lengths to simulate a real
    // pagemap file in procfs.
    pagemap_.fd_.reset(memfd_create("pagemap_test", MFD_CLOEXEC));
    ASSERT_TRUE(pagemap_.fd_.is_valid());
  }

  void TearDown() override {}

 protected:
  // Set the size of the pagemap appropriate for this many pages.
  void CreateStorageForPages(uint64_t pages,
                             void** start_address,
                             void** end_address) {
    ASSERT_NE(
        ftruncate(pagemap_.fd_.get(), pages * sizeof(Pagemap::PagemapEntry)),
        -1);
    *start_address = 0x0;
    *end_address =
        reinterpret_cast<char*>(*start_address) + base::GetPageSize() * pages;
  }

  void PutEntries(void* address, uint64_t* entries, size_t size) {
    ASSERT_EQ(HANDLE_EINTR(pwrite(
                  pagemap_.fd_.get(), entries, sizeof(entries[0]) * size,
                  reinterpret_cast<off_t>(address) / base::GetPageSize())),
              static_cast<ssize_t>(sizeof(entries[0]) * size));
  }

  Pagemap pagemap_{0};
};

// See: https://www.kernel.org/doc/Documentation/vm/pagemap.txt for more
// information.
//
// We make sure that we only have 55 bits set of the PFN which is why we and it
// with 2^55 - 1.
#define PFN(VALUE)                                                \
  ((reinterpret_cast<uint64_t>(reinterpret_cast<void*>(VALUE))) & \
   ((static_cast<uint64_t>(1) << 55) - 1))
#define SOFT_DIRTY static_cast<uint64_t>(1) << 55
#define EXCLUSIVELY_MAPPED static_cast<uint64_t>(1) << 56
#define FILE_PAGE static_cast<uint64_t>(1) << 61
#define SWAPPED static_cast<uint64_t>(1) << 62
#define PRESENT static_cast<uint64_t>(1) << 63

#define PFN_FROM_TYPE_AND_OFFSET(SWAP_TYPE, OFFSET) \
  ((SWAP_TYPE & 0x1F) | (OFFSET << 5))

TEST_F(PagemapTest, Basic) {
  void* start_address;
  void* end_address;
  CreateStorageForPages(4, &start_address, &end_address);

  uint64_t page_map_entries[] = {PFN(8675309) | SOFT_DIRTY | PRESENT,
                                 PFN(1234) | SWAPPED,
                                 PFN(5551212) | SOFT_DIRTY | FILE_PAGE,
                                 PFN(0xF00) | PRESENT | EXCLUSIVELY_MAPPED};
  PutEntries(start_address, page_map_entries, countof(page_map_entries));

  // We now have 4 pages populated in the page map from start address to 4 *
  // pagesize();

  // Validate that the Pagemap class can interpret them correctly.
  std::vector<Pagemap::PagemapEntry> entries;
  entries.resize(countof(page_map_entries));

  ASSERT_TRUE(pagemap_.GetEntries(
      reinterpret_cast<uint64_t>(start_address),
      countof(page_map_entries) * base::GetPageSize(), &entries));

  ASSERT_EQ(
      PFN_FROM_TYPE_AND_OFFSET(entries[0].swap_type, entries[0].swap_offset),
      8675309u);
  ASSERT_EQ(entries[0].pte_soft_dirty, true);
  ASSERT_EQ(entries[0].page_present, true);
  ASSERT_EQ(entries[0].page_swapped, false);
  ASSERT_EQ(entries[0].page_exclusively_mapped, false);
  ASSERT_EQ(entries[0].page_file_or_shared_anon, false);

  ASSERT_EQ(
      PFN_FROM_TYPE_AND_OFFSET(entries[1].swap_type, entries[1].swap_offset),
      1234u);
  ASSERT_EQ(entries[1].pte_soft_dirty, false);
  ASSERT_EQ(entries[1].page_present, false);
  ASSERT_EQ(entries[1].page_swapped, true);
  ASSERT_EQ(entries[1].page_exclusively_mapped, false);
  ASSERT_EQ(entries[1].page_file_or_shared_anon, false);

  ASSERT_EQ(
      PFN_FROM_TYPE_AND_OFFSET(entries[2].swap_type, entries[2].swap_offset),
      5551212u);
  ASSERT_EQ(entries[2].pte_soft_dirty, true);
  ASSERT_EQ(entries[2].page_present, false);
  ASSERT_EQ(entries[2].page_swapped, false);
  ASSERT_EQ(entries[2].page_exclusively_mapped, false);
  ASSERT_EQ(entries[2].page_file_or_shared_anon, true);

  ASSERT_EQ(
      PFN_FROM_TYPE_AND_OFFSET(entries[3].swap_type, entries[3].swap_offset),
      0xF00u);
  ASSERT_EQ(entries[3].pte_soft_dirty, false);
  ASSERT_EQ(entries[3].page_present, true);
  ASSERT_EQ(entries[3].page_swapped, false);
  ASSERT_EQ(entries[3].page_exclusively_mapped, true);
  ASSERT_EQ(entries[3].page_file_or_shared_anon, false);
}

TEST_F(PagemapTest, MidRangeRead) {
  // This test will validate that we can read only a portion of the pages.
  void* start_address;
  void* end_address;
  CreateStorageForPages(4, &start_address, &end_address);

  uint64_t page_map_entries[] = {PFN(8675309) | SOFT_DIRTY | PRESENT,
                                 PFN(1234) | SWAPPED,
                                 PFN(5551212) | SOFT_DIRTY | FILE_PAGE,
                                 PFN(0xF00) | PRESENT | EXCLUSIVELY_MAPPED};
  PutEntries(start_address, page_map_entries, countof(page_map_entries));

  // We will read the two middle pages.
  std::vector<Pagemap::PagemapEntry> entries;
  ASSERT_TRUE(pagemap_.GetEntries(
      reinterpret_cast<uint64_t>(start_address) + base::GetPageSize(),
      2 * base::GetPageSize(), &entries));
  ASSERT_EQ(entries.size(), 2u);

  ASSERT_EQ(
      PFN_FROM_TYPE_AND_OFFSET(entries[0].swap_type, entries[0].swap_offset),
      1234u);
  ASSERT_EQ(entries[0].pte_soft_dirty, false);
  ASSERT_EQ(entries[0].page_present, false);
  ASSERT_EQ(entries[0].page_swapped, true);
  ASSERT_EQ(entries[0].page_exclusively_mapped, false);
  ASSERT_EQ(entries[0].page_file_or_shared_anon, false);

  ASSERT_EQ(
      PFN_FROM_TYPE_AND_OFFSET(entries[1].swap_type, entries[1].swap_offset),
      5551212u);
  ASSERT_EQ(entries[1].pte_soft_dirty, true);
  ASSERT_EQ(entries[1].page_present, false);
  ASSERT_EQ(entries[1].page_swapped, false);
  ASSERT_EQ(entries[1].page_exclusively_mapped, false);
  ASSERT_EQ(entries[1].page_file_or_shared_anon, true);
}

TEST_F(PagemapTest, VectorResizedWhenIncorrectlySized) {
  // This test validates that passing in an incorrectly sized vector is handled
  // automatically.
  void* start_address;
  void* end_address;
  CreateStorageForPages(4, &start_address, &end_address);

  uint64_t page_map_entries[] = {PFN(8675309) | SOFT_DIRTY | PRESENT,
                                 PFN(1234) | SWAPPED,
                                 PFN(5551212) | SOFT_DIRTY | FILE_PAGE,
                                 PFN(0xF00) | PRESENT | EXCLUSIVELY_MAPPED};
  PutEntries(start_address, page_map_entries, countof(page_map_entries));

  // We will read the two middle pages.
  std::vector<Pagemap::PagemapEntry> entries;
  entries.resize(1);  // This is too short, it will be automatically resized.

  ASSERT_TRUE(pagemap_.GetEntries(
      reinterpret_cast<uint64_t>(start_address) + base::GetPageSize(),
      2 * base::GetPageSize(), &entries));
  ASSERT_EQ(entries.size(), 2u);

  ASSERT_EQ(
      PFN_FROM_TYPE_AND_OFFSET(entries[0].swap_type, entries[0].swap_offset),
      1234u);
  ASSERT_EQ(entries[0].pte_soft_dirty, false);
  ASSERT_EQ(entries[0].page_present, false);
  ASSERT_EQ(entries[0].page_swapped, true);
  ASSERT_EQ(entries[0].page_exclusively_mapped, false);
  ASSERT_EQ(entries[0].page_file_or_shared_anon, false);

  ASSERT_EQ(
      PFN_FROM_TYPE_AND_OFFSET(entries[1].swap_type, entries[1].swap_offset),
      5551212u);
  ASSERT_EQ(entries[1].pte_soft_dirty, true);
  ASSERT_EQ(entries[1].page_present, false);
  ASSERT_EQ(entries[1].page_swapped, false);
  ASSERT_EQ(entries[1].page_exclusively_mapped, false);
  ASSERT_EQ(entries[1].page_file_or_shared_anon, true);
}

}  // namespace memory
}  // namespace ash
