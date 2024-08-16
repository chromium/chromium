// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/updater/certificate_tag.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "chrome/updater/certificate_tag_internal.h"
#include "chrome/updater/test/unit_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace updater::tagging {

TEST(CertificateTag, RoundTrip) {
  std::string exe;
  ASSERT_TRUE(base::ReadFileToString(
      updater::test::GetTestFilePath("signed.exe.gz"), &exe));
  ASSERT_TRUE(compression::GzipUncompress(exe, &exe));
  const base::span<const uint8_t> exe_span(
      reinterpret_cast<const uint8_t*>(exe.data()), exe.size());

  std::unique_ptr<BinaryInterface> bin(CreatePEBinary(exe_span));
  ASSERT_TRUE(bin);

  // Binary should be untagged on disk.
  std::optional<std::vector<uint8_t>> orig_tag(bin->tag());
  EXPECT_FALSE(orig_tag);

  constexpr uint8_t kTag[] = {1, 2, 3, 4, 5};
  std::optional<std::vector<uint8_t>> updated_exe(bin->SetTag(kTag));
  ASSERT_TRUE(updated_exe);

  std::unique_ptr<BinaryInterface> bin2(CreatePEBinary(*updated_exe));
  ASSERT_TRUE(bin2);
  std::optional<std::vector<uint8_t>> parsed_tag(bin2->tag());
  ASSERT_TRUE(parsed_tag);
  ASSERT_EQ(parsed_tag->size(), sizeof(kTag));
  EXPECT_TRUE(memcmp(kTag, parsed_tag->data(), sizeof(kTag)) == 0);

  // Update an existing tag.
  constexpr uint8_t kTag2[] = {1, 2, 3, 4, 6};
  std::optional<std::vector<uint8_t>> updated_again_exe(bin2->SetTag(kTag2));
  ASSERT_TRUE(updated_again_exe);

  std::unique_ptr<BinaryInterface> bin3(CreatePEBinary(*updated_again_exe));
  ASSERT_TRUE(bin3);
  std::optional<std::vector<uint8_t>> parsed_tag2(bin3->tag());
  ASSERT_TRUE(parsed_tag2);
  ASSERT_EQ(parsed_tag2->size(), sizeof(kTag2));
  EXPECT_TRUE(memcmp(kTag2, parsed_tag2->data(), sizeof(kTag2)) == 0);

  // Updating an existing tag with a tag of the same size should not have grown
  // the binary, i.e. the old tag should have been erased first.
  EXPECT_EQ(sizeof(kTag), sizeof(kTag2));
  EXPECT_EQ(updated_exe->size(), updated_again_exe->size());
}

namespace internal {

struct CertificateTagMsiIsLastInSectorTestCase {
  const int index;

  // 12 for 4096-byte sectors, 9 for 512-byte sectors.
  const uint16_t sector_shift;
  const bool expected_is_last_in_sector;
};

class CertificateTagMsiIsLastInSectorTest
    : public ::testing::TestWithParam<CertificateTagMsiIsLastInSectorTestCase> {
};

INSTANTIATE_TEST_SUITE_P(
    CertificateTagMsiIsLastInSectorTestCases,
    CertificateTagMsiIsLastInSectorTest,
    ::testing::ValuesIn(std::vector<CertificateTagMsiIsLastInSectorTestCase>{
        {0, 12, false},
        {1, 12, false},
        {107, 12, false},
        {108, 12, false},
        {109, 12, false},
        {1131, 12, false},
        {1132, 12, true},
        {1133, 12, false},
        {2156, 12, true},
        {0, 9, false},
        {1, 9, false},
        {107, 9, false},
        {108, 9, false},
        {109, 9, false},
        {236, 9, true},
        {364, 9, true},
    }));

TEST_P(CertificateTagMsiIsLastInSectorTest, TestCases) {
  EXPECT_EQ(IsLastInSector(*NewSectorFormat(GetParam().sector_shift),
                           GetParam().index),
            GetParam().expected_is_last_in_sector);
}

struct CertificateTagMsiFirstFreeFatEntryTestCase {
  const int index;
  const uint32_t expected_first_free_fat_entry;
};

class CertificateTagMsiFirstFreeFatEntryTest
    : public ::testing::TestWithParam<
          CertificateTagMsiFirstFreeFatEntryTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    CertificateTagMsiFirstFreeFatEntryTestCases,
    CertificateTagMsiFirstFreeFatEntryTest,
    ::testing::ValuesIn(std::vector<CertificateTagMsiFirstFreeFatEntryTestCase>{
        {1023, 1024},
        {1000, 1001},
        {10, 11},
        {0, 1},
    }));

TEST_P(CertificateTagMsiFirstFreeFatEntryTest, TestCases) {
  std::vector<uint32_t> entries(1024, kFatFreeSector);
  entries[GetParam().index] = 1;
  EXPECT_EQ(MSIBinary::FirstFreeFatEntry(entries),
            GetParam().expected_first_free_fat_entry);
}

std::vector<uint32_t> GetFatEntries(int sectors, int free_entries) {
  // 1024 int32 entries per sector.
  const int used_entries = 1024 * sectors - free_entries;

  // Initialize `entries` to zeroes. Zero is a valid sector. In a valid file
  // though, the sector number would not repeat zeroes like this, but this works
  // for the unit tests.
  std::vector<uint32_t> entries(used_entries);
  entries.reserve(used_entries + free_entries);

  // Set a non-contiguous free sector before the end, which should not affect
  // anything.
  if (used_entries > 2) {
    entries[used_entries - 2] = kFatFreeSector;
  }
  for (int i = 0; i < free_entries; ++i) {
    entries.push_back(kFatFreeSector);
  }
  return entries;
}

std::vector<uint32_t> GetDifatEntries(int sectors, int free_entries) {
  // Similar to GetFatEntries, but there are always 109 non-sector elements from
  // the header. The last element of any sectors should be `kFatEndOfChain` or a
  // sector number.
  // https://learn.microsoft.com/en-us/openspecs/windows_protocols/
  // ms-cfb/0afa4e43-b18f-432a-9917-4f276eca7a73
  std::vector<uint32_t> entries(kNumDifatHeaderEntries);
  entries.reserve(kNumDifatHeaderEntries + sectors * 1024);

  // Some sector number.
  const uint32_t sentinel = 123;
  for (; sectors > 0; --sectors) {
    std::vector<uint32_t> new_entries(1024);
    new_entries[1023] = sectors == 1 ? kFatEndOfChain : sentinel;
    entries.insert(entries.end(), new_entries.begin(), new_entries.end());
  }
  size_t i = entries.size() - 1;
  while (free_entries > 0) {
    if (entries[i] != kFatEndOfChain && entries[i] != sentinel) {
      entries[i] = kFatFreeSector;
      --free_entries;
    }
    --i;
  }
  return entries;
}

MSIBinary GetMsiBinary(const std::vector<uint32_t>& fat_entries,
                       const std::vector<uint32_t>& difat_entries) {
  // Uses dll version 4, 4096-byte sectors.
  // There are difat sectors only if difat_entries.size() > 109.
  const size_t n =
      difat_entries.size() > kNumDifatHeaderEntries
          ? (difat_entries.size() - kNumDifatHeaderEntries - 1) / 1024 + 1
          : 0;

  MSIHeader header;
  header.dll_version = 4;
  header.sector_shift = 12;
  header.num_difat_sectors = n;

  MSIBinary bin;
  bin.header_ = header;
  bin.sector_format_ = *NewSectorFormat(12);
  bin.fat_entries_ = fat_entries;
  bin.difat_entries_ = difat_entries;
  bin.difat_sectors_.resize(n);
  return bin;
}

std::vector<uint32_t> VerifyAndRemoveDifatChaining(
    std::vector<uint32_t> entries) {
  const SectorFormat format = *NewSectorFormat(12);
  for (int i = entries.size() - 1; i >= 0; --i) {
    if (IsLastInSector(format, i)) {
      if (static_cast<size_t>(i) == entries.size() - 1) {
        EXPECT_EQ(entries[i], kFatEndOfChain);
      } else {
        EXPECT_LT(entries[i], kFatReserved);
      }
      entries.erase(entries.begin() + i);
    }
  }
  return entries;
}

void VerifyEntries(size_t added,
                   const std::vector<uint32_t>& changed_entries,
                   std::vector<uint32_t> old_entries,
                   std::vector<uint32_t> new_entries,
                   bool is_difat) {
  ASSERT_EQ(new_entries.size() - old_entries.size(), added);

  // If this is difat, check and remove the chaining entries. This simplifies
  // the checks below.
  if (is_difat) {
    // If there is an error in `old_entries`, the test case was not set up
    // correctly.
    old_entries = VerifyAndRemoveDifatChaining(old_entries);
    new_entries = VerifyAndRemoveDifatChaining(new_entries);
  }

  // Can be past end of slice.
  size_t first_free = old_entries.size();
  while (first_free > 0 && old_entries[first_free - 1] == kFatFreeSector) {
    --first_free;
  }
  const std::vector<uint32_t> same_entries = std::vector<uint32_t>(
      new_entries.begin(), new_entries.begin() + first_free);
  const std::vector<uint32_t> diff_entries = std::vector<uint32_t>(
      new_entries.begin() + first_free,
      new_entries.begin() + first_free + changed_entries.size());
  const std::vector<uint32_t> free_entries = std::vector<uint32_t>(
      new_entries.begin() + first_free + changed_entries.size(),
      new_entries.end());
  for (size_t i = 0; i < same_entries.size(); ++i) {
    EXPECT_EQ(old_entries[i], same_entries[i]);
  }
  for (size_t i = 0; i < diff_entries.size(); ++i) {
    EXPECT_EQ(changed_entries[i], diff_entries[i]);
  }
  for (size_t i = 0; i < free_entries.size(); ++i) {
    EXPECT_EQ(free_entries[i], kFatFreeSector);
  }
}

struct CertificateTagMsiEnsureFreeDifatEntryTestCase {
  const int difat_sectors;
  const int num_free_difat_entries;
  const std::vector<uint32_t> expected_changed_difat_entries;
  const size_t expected_num_difat_entries_added;
  const int num_fat_sectors;
  const int num_free_fat_entries;
  const std::vector<uint32_t> expected_changed_fat_entries;
  const size_t expected_num_fat_entries_added;
};

class CertificateTagMsiEnsureFreeDifatEntryTest
    : public ::testing::TestWithParam<
          CertificateTagMsiEnsureFreeDifatEntryTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    CertificateTagMsiEnsureFreeDifatEntryTestCases,
    CertificateTagMsiEnsureFreeDifatEntryTest,
    ::testing::ValuesIn(
        std::vector<CertificateTagMsiEnsureFreeDifatEntryTestCase>{
            // Note: The number of difat used entries should imply the # of fat
            // sectors.
            // But that inconsistency doesn't affect these tests.

            // Free difat entry in header, no change.
            {0, 108, {}, 0, 1, 40, {}, 0},

            // No free difat entry, add a difat sector (1024 entries).
            {0, 0, {}, 1024, 1, 40, {kFatDifSector}, 0},

            // Free difat entry in sector, no change.
            {1, 1, {}, 0, 1, 40, {}, 0},

            // No free difat entry, add a difat sector.
            {1, 0, {}, 1024, 1, 40, {kFatDifSector}, 0},

            // Additional sector is completely empty, no change.
            {1, 1023, {}, 0, 1, 40, {}, 0},

            // Free difat entry; No free fat entry. No change to either.
            {0, 10, {}, 0, 1, 0, {}, 0},

            // No free difat entry; add a difat sector. No free fat entry; add a
            // fat sector.
            {0, 0, {1024}, 1024, 1, 0, {kFatFatSector, kFatDifSector}, 1024},
            {1, 0, {1024}, 1024, 1, 0, {kFatFatSector, kFatDifSector}, 1024},
        }));

TEST_P(CertificateTagMsiEnsureFreeDifatEntryTest, TestCases) {
  const std::vector<uint32_t> fat_entries = GetFatEntries(
      GetParam().num_fat_sectors, GetParam().num_free_fat_entries);
  const std::vector<uint32_t> difat_entries = GetDifatEntries(
      GetParam().difat_sectors, GetParam().num_free_difat_entries);
  MSIBinary bin = GetMsiBinary(fat_entries, difat_entries);
  bin.EnsureFreeDifatEntry();

  // Check added entries.
  VerifyEntries(GetParam().expected_num_difat_entries_added,
                GetParam().expected_changed_difat_entries, difat_entries,
                bin.difat_entries_, true);
  VerifyEntries(GetParam().expected_num_fat_entries_added,
                GetParam().expected_changed_fat_entries, fat_entries,
                bin.fat_entries_, false);
}

struct CertificateTagMsiEnsureFreeFatEntriesTestCase {
  const int difat_sectors;
  const int num_free_difat_entries;
  const std::vector<uint32_t> expected_changed_difat_entries;
  const size_t expected_num_difat_entries_added;
  const int num_fat_sectors;
  const int num_free_fat_entries;
  const uint32_t num_fat_entries_requested;
  const std::vector<uint32_t> expected_changed_fat_entries;
  const size_t expected_num_fat_entries_added;
};

class CertificateTagMsiEnsureFreeFatEntriesTest
    : public ::testing::TestWithParam<
          CertificateTagMsiEnsureFreeFatEntriesTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    CertificateTagMsiEnsureFreeFatEntriesTestCases,
    CertificateTagMsiEnsureFreeFatEntriesTest,
    ::testing::ValuesIn(std::vector<
                        CertificateTagMsiEnsureFreeFatEntriesTestCase>{
        // Note: The number of difat used entries should imply the # of fat
        // sectors.
        // But that inconsistency doesn't affect these tests.

        {0, 1, {}, 0, 1, 2, 2, {}, 0},
        {0, 0, {}, 0, 1, 2, 2, {}, 0},
        {0, 1, {1022}, 0, 1, 2, 4, {kFatFatSector}, 1024},
        {0, 0, {1022}, 1024, 1, 2, 4, {kFatFatSector, kFatDifSector}, 1024},
        {0, 1, {1024}, 0, 1, 0, 4, {kFatFatSector}, 1024},
        {0, 0, {1024}, 1024, 1, 0, 4, {kFatFatSector, kFatDifSector}, 1024},
        {1, 1, {1022}, 0, 1, 2, 4, {kFatFatSector}, 1024},
        {1, 0, {1022}, 1024, 1, 2, 4, {kFatFatSector, kFatDifSector}, 1024},
        {2, 1, {2046}, 0, 2, 2, 4, {kFatFatSector}, 1024},
        {2, 0, {2046}, 1024, 2, 2, 4, {kFatFatSector, kFatDifSector}, 1024},

        // These are unlikely cases, but they should work.
        // Request exactly one more sector free. The difat sector will consume
        // a fat entry as well.
        {0, 1, {1022}, 0, 1, 2, 1025, {kFatFatSector}, 1024},

        // Request more than one more sector.
        {0,
         2,
         {1022, 1023},
         0,
         1,
         2,
         1026,
         {kFatFatSector, kFatFatSector},
         2048},

        // Request more than one sector because of additional difat sector.
        {0,
         0,
         {1022, 1024},
         1024,
         1,
         2,
         1025,
         {kFatFatSector, kFatDifSector, kFatFatSector},
         2048},
    }));

TEST_P(CertificateTagMsiEnsureFreeFatEntriesTest, TestCases) {
  const std::vector<uint32_t> fat_entries = GetFatEntries(
      GetParam().num_fat_sectors, GetParam().num_free_fat_entries);
  const std::vector<uint32_t> difat_entries = GetDifatEntries(
      GetParam().difat_sectors, GetParam().num_free_difat_entries);
  MSIBinary bin = GetMsiBinary(fat_entries, difat_entries);
  bin.EnsureFreeFatEntries(GetParam().num_fat_entries_requested);

  // Check added entries.
  VerifyEntries(GetParam().expected_num_difat_entries_added,
                GetParam().expected_changed_difat_entries, difat_entries,
                bin.difat_entries_, true);
  VerifyEntries(GetParam().expected_num_fat_entries_added,
                GetParam().expected_changed_fat_entries, fat_entries,
                bin.fat_entries_, false);
}

struct CertificateTagMsiAssignDifatEntryTestCase {
  const int difat_sectors;
  const int num_free_difat_entries;
  const size_t difat_entries_assigned_index;
  const uint32_t difat_entry_assigned_value;
  const int num_fat_sectors;
  const int num_free_fat_entries;
};

class CertificateTagMsiAssignDifatEntryTest
    : public ::testing::TestWithParam<
          CertificateTagMsiAssignDifatEntryTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    CertificateTagMsiAssignDifatEntryTestCases,
    CertificateTagMsiAssignDifatEntryTest,
    ::testing::ValuesIn(std::vector<CertificateTagMsiAssignDifatEntryTestCase>{
        {0, 1, 108, 1000, 1, 23},
        {0, 0, 109, 1000, 1, 23},
        {1, 1, 1131, 1000, 1, 23},
        {1, 0, 1133, 1000, 1, 23},
    }));

TEST_P(CertificateTagMsiAssignDifatEntryTest, TestCases) {
  const std::vector<uint32_t> fat_entries = GetFatEntries(
      GetParam().num_fat_sectors, GetParam().num_free_fat_entries);
  const std::vector<uint32_t> difat_entries = GetDifatEntries(
      GetParam().difat_sectors, GetParam().num_free_difat_entries);
  MSIBinary bin = GetMsiBinary(fat_entries, difat_entries);
  bin.AssignDifatEntry(GetParam().difat_entry_assigned_value);

  ASSERT_GE(bin.difat_entries_.size(),
            GetParam().difat_entries_assigned_index + 1);
  ASSERT_EQ(bin.difat_entries_[GetParam().difat_entries_assigned_index],
            GetParam().difat_entry_assigned_value);
}

// Checks the provided `bin` MSIBinary for internal consistency. Additionally,
// if `other` MSIBinary is provided, checks that the data streams in `other` are
// bitwise identical to `bin`.
void Validate(const MSIBinary& bin,
              std::optional<std::reference_wrapper<const MSIBinary>> other) {
  // Check the fat sector entries.
  uint64_t i = 0;
  for (const auto s : bin.difat_entries_) {
    ASSERT_TRUE(s == kFatFreeSector || IsLastInSector(bin.sector_format_, i) ||
                bin.fat_entries_[s] == kFatFatSector);
    ++i;
  }

  // Check the difat sector entries.
  i = kNumDifatHeaderEntries - 1;
  uint32_t num = 0;
  for (uint32_t s = bin.header_.first_difat_sector; s != kFatEndOfChain;
       ++num) {
    ASSERT_EQ(bin.fat_entries_[s], kFatDifSector);
    i += bin.sector_format_.ints;
    s = bin.difat_entries_[i];
  }
  ASSERT_EQ(num, bin.header_.num_difat_sectors);

  // Enumerate the directory entries.
  // * Validate streams in the fat: Walk the chain, validate the stream length,
  //   and mark sectors in a copy of the fat so we can tell if any sectors are
  //   re-used.
  // * Compare bytes in the data streams, to validate none of them changed.
  //   We could match stream names, but the directory entries are not reordered
  //   and the streams are not moved.
  std::vector<uint32_t> fat_entries = bin.fat_entries_;
  uint32_t dir_sector = bin.header_.first_dir_sector;
  MSIDirEntry entry;
  do {
    // Fixed `kNumDirEntryBytes` directory entry size.
    for (i = 0; i < bin.sector_format_.size / kNumDirEntryBytes; ++i) {
      uint64_t offset =
          dir_sector * bin.sector_format_.size + i * kNumDirEntryBytes;
      std::memcpy(&entry, &bin.contents_[offset], sizeof(MSIDirEntry));

      // Skip the mini stream and signature entries.
      if (entry.stream_size < kMiniStreamCutoffSize ||
          std::equal(entry.name, entry.name + entry.num_name_bytes,
                     std::begin(kSignatureName))) {
        continue;
      }
      uint64_t allocated_size = 0;
      uint32_t sector = entry.stream_first_sector;
      uint32_t next = kFatEndOfChain;
      do {
        allocated_size += bin.sector_format_.size;
        ASSERT_TRUE(fat_entries[sector] == kFatEndOfChain ||
                    fat_entries[sector] < kFatReserved);
        if (other) {
          offset = sector * bin.sector_format_.size;
          ASSERT_TRUE(std::equal(
              bin.contents_.begin() + offset,
              bin.contents_.begin() + offset + bin.sector_format_.size,
              other->get().contents_.begin() + offset));
        }
        next = fat_entries[sector];

        // Overwrite the fat entry and detect if the entry is re-used.
        fat_entries[sector] = kFatReserved;
        sector = next;
      } while (next != kFatEndOfChain);
      ASSERT_GE(allocated_size, entry.stream_size);
    }

    // Go to the next directory sector.
    dir_sector = bin.fat_entries_[dir_sector];
  } while (dir_sector != kFatEndOfChain);
}

struct CertificateTagMsiValidateTestCase {
  const std::string infile;
  const std::vector<uint8_t> expected_tag;
  const std::vector<uint8_t> new_tag;
};

class CertificateTagMsiValidateTest
    : public ::testing::TestWithParam<CertificateTagMsiValidateTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    CertificateTagMsiValidateTestCases,
    CertificateTagMsiValidateTest,
    ::testing::ValuesIn(std::vector<CertificateTagMsiValidateTestCase>{
        {"GUH-untagged.msi", {}, {1, 2, 3, 4, 5}},
        {"GUH-brand-only.msi",
         [] {
           std::vector<uint8_t> expected_tag = {
               'G', 'a', 'c', 't',  '2',  '.', '0', 'O', 'm',
               'a', 'h', 'a', '\0', '\n', 'b', 'r', 'a', 'n',
               'd', '=', 'Q', 'A',  'Q',  'A', '\0'};
           expected_tag.resize(8240);
           return expected_tag;
         }(),
         {1, 2, 3, 4, 5}},
        {"GUH-multiple.msi",
         [] {
           std::vector<uint8_t> expected_tag(8632);
           constexpr char magic[] = "Gact2.0Omaha";
           std::memcpy(&expected_tag[0], magic, sizeof(magic));
           constexpr char tag[] =
               "appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&iid={2D8C18E9-"
               "8D3A-4EFC-6D61-AE23E3530EA2}&lang=en&browser=4&usagestats=0&"
               "appname=Google%20Chrome&needsadmin=prefers&brand=CHMB&"
               "installdataindex=defaultbrowser";
           expected_tag[sizeof(magic)] = sizeof(tag) - 1;
           std::memcpy(&expected_tag[sizeof(magic) + 1], tag, sizeof(tag));
           return expected_tag;
         }(),
         [] {
           std::vector<uint8_t> new_tag = {'G', 'a', 'c', 't', '2', '.',  '0',
                                           'O', 'm', 'a', 'h', 'a', '\0', '\n',
                                           'b', 'r', 'a', 'n', 'd', '=',  'Q',
                                           'A', 'Q', 'A', '\0'};
           new_tag.resize(8206);
           return new_tag;
         }()},
    }));

TEST_P(CertificateTagMsiValidateTest, TestCases) {
  base::MemoryMappedFile mapped_file;
  ASSERT_TRUE(mapped_file.Initialize(
      test::GetTestFilePath("tagged_msi").AppendASCII(GetParam().infile)));
  const std::unique_ptr<MSIBinary> bin = MSIBinary::Parse(mapped_file.bytes());
  ASSERT_TRUE(bin);

  if (GetParam().expected_tag.empty()) {
    ASSERT_FALSE(bin->tag());
  } else {
    ASSERT_EQ(*bin->tag(), GetParam().expected_tag);
  }
  ASSERT_NO_FATAL_FAILURE(Validate(*bin, {}));

  // Set a new tag on `bin`.
  const std::optional<std::vector<uint8_t>> tagged_bytes =
      bin->SetTag(GetParam().new_tag);
  ASSERT_TRUE(tagged_bytes);
  const std::unique_ptr<MSIBinary> tagged_bin = MSIBinary::Parse(*tagged_bytes);
  ASSERT_TRUE(tagged_bin);

  if (GetParam().new_tag.empty()) {
    ASSERT_FALSE(tagged_bin->tag());
  } else {
    ASSERT_EQ(*tagged_bin->tag(), GetParam().new_tag);
  }
  ASSERT_NO_FATAL_FAILURE(Validate(*bin, *tagged_bin));
}

}  // namespace internal

}  // namespace updater::tagging
