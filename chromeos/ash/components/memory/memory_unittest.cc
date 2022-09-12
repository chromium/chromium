// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/memory.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace memory {

TEST(MemoryTest, ZramMmStat) {
  ZramMmStat zram_mm_stat;

  std::string zramMmStatContent =
      "    4096       74    12288        0    12288        0        0        0 "
      "       0";
  ASSERT_TRUE(internal::ParseZramMmStat(zramMmStatContent, &zram_mm_stat));
  ASSERT_EQ(zram_mm_stat.orig_data_size, 4096u);
  ASSERT_EQ(zram_mm_stat.compr_data_size, 74u);
  ASSERT_EQ(zram_mm_stat.mem_used_total, 12288u);
  ASSERT_EQ(zram_mm_stat.mem_limit, 0u);
  ASSERT_EQ(zram_mm_stat.mem_used_max, 12288u);
  ASSERT_EQ(zram_mm_stat.same_pages, 0u);
  ASSERT_EQ(zram_mm_stat.pages_compacted, 0u);
  ASSERT_EQ(zram_mm_stat.huge_pages, 0u);
  ASSERT_EQ(zram_mm_stat.huge_pages_since, 0u);

  // mm_stat only contains number.
  zramMmStatContent =
      "    aa4096    bb74    122e8  gg0    12288        0        0        0    "
      "    0";
  ASSERT_FALSE(internal::ParseZramMmStat(zramMmStatContent, &zram_mm_stat));

  // mm_stat contains at least 7 items.
  zramMmStatContent = "    0        0        0        0";
  ASSERT_FALSE(internal::ParseZramMmStat(zramMmStatContent, &zram_mm_stat));

  // The fifth item in mm_stat must be positive.
  zramMmStatContent =
      "    4096       74    12288        0    -12288        0        0        "
      "0        0";
  ASSERT_FALSE(internal::ParseZramMmStat(zramMmStatContent, &zram_mm_stat));
}

TEST(MemoryTest, ZramBdStat) {
  ZramBdStat zram_bd_stat;

  std::string zramBdStatContent = "       0        0        0";
  ASSERT_TRUE(internal::ParseZramBdStat(zramBdStatContent, &zram_bd_stat));
  ASSERT_EQ(zram_bd_stat.bd_count, 0u);
  ASSERT_EQ(zram_bd_stat.bd_reads, 0u);
  ASSERT_EQ(zram_bd_stat.bd_writes, 0u);

  // bd_stat only contains number.
  zramBdStatContent = "    aa4096    bb74    122e8";
  ASSERT_FALSE(internal::ParseZramBdStat(zramBdStatContent, &zram_bd_stat));

  // bd_stat contains 3 items.
  zramBdStatContent = "    0        0";
  ASSERT_FALSE(internal::ParseZramBdStat(zramBdStatContent, &zram_bd_stat));
}

TEST(MemoryTest, ZramIoStat) {
  ZramIoStat zram_io_stat;

  std::string zramIoStatContent = "       0        0        0        0";
  ASSERT_TRUE(internal::ParseZramIoStat(zramIoStatContent, &zram_io_stat));
  ASSERT_EQ(zram_io_stat.failed_reads, 0u);
  ASSERT_EQ(zram_io_stat.failed_writes, 0u);
  ASSERT_EQ(zram_io_stat.invalid_io, 0u);
  ASSERT_EQ(zram_io_stat.notify_free, 0u);

  // io_stat only contains number.
  zramIoStatContent = "    aa4096    bb74    122e8  gg0";
  ASSERT_FALSE(internal::ParseZramIoStat(zramIoStatContent, &zram_io_stat));

  // io_stat contains 4 items.
  zramIoStatContent = "    0        0        0";
  ASSERT_FALSE(internal::ParseZramIoStat(zramIoStatContent, &zram_io_stat));
}

}  // namespace memory
}  // namespace ash
