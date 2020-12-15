// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/pressure/pressure.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(MemoryPressureTest, CalculateReservedFreeKB) {
  const std::string kMockPartialZoneinfo(R"(
Node 0, zone      DMA
  pages free     3968
        min      137
        low      171
        high     205
        spanned  4095
        present  3999
        managed  3976
        protection: (0, 1832, 3000, 3786)
Node 0, zone    DMA32
  pages free     422432
        min      16270
        low      20337
        high     24404
        spanned  1044480
        present  485541
        managed  469149
        protection: (0, 0, 1953, 1500)
Node 0, zone   Normal
  pages free     21708
        min      17383
        low      21728
        high     26073
        spanned  524288
        present  524288
        managed  501235
        protection: (0, 0, 0, 0))");
  constexpr uint64_t kPageSizeKB = 4;
  const uint64_t high_watermarks = 205 + 24404 + 26073;
  const uint64_t lowmem_reserves = 3786 + 1953 + 0;
  const uint64_t reserved =
      chromeos::memory::pressure::CalculateReservedFreeKB(kMockPartialZoneinfo);
  ASSERT_EQ(reserved, (high_watermarks + lowmem_reserves) * kPageSizeKB);
}

TEST(MemoryPressureTest, ParsePSIMemory) {
  const std::string kMockPSIMemory(R"(
some avg10=57.25 avg60=35.97 avg300=10.18 total=32748793
full avg10=29.29 avg60=19.01 avg300=5.44 total=17589167)");
  const double pressure =
      chromeos::memory::pressure::ParsePSIMemory(kMockPSIMemory);
  ASSERT_EQ(pressure, 57.25);
}

TEST(MemoryPressureTest, CalculateAvailableMemoryUserSpaceKB) {
  base::SystemMemoryInfoKB info;
  uint64_t available;
  const uint64_t min_filelist = 400 * 1024;
  const uint64_t reserved_free = 0;
  const uint64_t ram_swap_weight = 4;

  // Available determined by file cache.
  info.inactive_file = 500 * 1024;
  info.active_file = 500 * 1024;
  available = chromeos::memory::pressure::CalculateAvailableMemoryUserSpaceKB(
      info, reserved_free, min_filelist, ram_swap_weight);
  ASSERT_EQ(available, 1000 * 1024 - min_filelist);

  // Available determined by swap free.
  info.swap_free = 1200 * 1024;
  info.inactive_anon = 1000 * 1024;
  info.active_anon = 1000 * 1024;
  info.inactive_file = 0;
  info.active_file = 0;
  available = chromeos::memory::pressure::CalculateAvailableMemoryUserSpaceKB(
      info, reserved_free, min_filelist, ram_swap_weight);
  ASSERT_EQ(available, uint64_t(300 * 1024));

  // Available determined by anonymous.
  info.swap_free = 6000 * 1024;
  info.inactive_anon = 500 * 1024;
  info.active_anon = 500 * 1024;
  available = chromeos::memory::pressure::CalculateAvailableMemoryUserSpaceKB(
      info, reserved_free, min_filelist, ram_swap_weight);
  ASSERT_EQ(available, uint64_t(250 * 1024));
}

TEST(MemoryPressureTest, ParseMarginFileGood) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());

  base::FilePath margin_file = tmp_dir.GetPath().Append("margin");

  ASSERT_TRUE(base::WriteFile(margin_file, "123"));
  const std::vector<uint64_t> parts1 =
      chromeos::memory::pressure::GetMarginFileParts(margin_file.value());
  ASSERT_EQ(1u, parts1.size());
  ASSERT_EQ(123u, parts1[0]);

  ASSERT_TRUE(base::WriteFile(margin_file, "123 456"));
  const std::vector<uint64_t> parts2 =
      chromeos::memory::pressure::GetMarginFileParts(margin_file.value());
  ASSERT_EQ(2u, parts2.size());
  ASSERT_EQ(123u, parts2[0]);
  ASSERT_EQ(456u, parts2[1]);
}

TEST(MemoryPressureTest, ParseMarginFileBad) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::FilePath margin_file = tmp_dir.GetPath().Append("margin");

  // An empty margin file is bad.
  ASSERT_TRUE(base::WriteFile(margin_file, ""));
  ASSERT_TRUE(
      chromeos::memory::pressure::GetMarginFileParts(margin_file.value())
          .empty());

  // The numbers will be in base10, so 4a6 would be invalid.
  ASSERT_TRUE(base::WriteFile(margin_file, "123 4a6"));
  ASSERT_TRUE(
      chromeos::memory::pressure::GetMarginFileParts(margin_file.value())
          .empty());

  // The numbers must be integers.
  ASSERT_TRUE(base::WriteFile(margin_file, "123.2 412.3"));
  ASSERT_TRUE(
      chromeos::memory::pressure::GetMarginFileParts(margin_file.value())
          .empty());
}
