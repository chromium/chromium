// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/mac/udif.h"

#include <hfs/hfs_format.h>
#include <libkern/OSByteOrder.h>
#include <stddef.h>
#include <stdint.h>

#include <array>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/stringprintf.h"
#include "chrome/utility/safe_browsing/mac/dmg_test_utils.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace dmg {
namespace {

constexpr std::array<const std::string_view, 8> kGPTExpectedPartitions = {
    "MBR",
    "Primary GPT Header",
    "Primary GPT Tabler",
    "Apple_Free",
    "Apple_HFS",
    "Apple_Free",
    "Backup GPT Table",
    "Backup GPT Header"};

constexpr std::array<std::string_view, 1> kNoPartitionMap = {"Apple_HFS"};

constexpr std::array<std::string_view, 4> kAPMExpectedPartitions = {
    "DDM",
    "Apple_partition_map",
    "Apple_HFS",
    "Apple_Free",
};

struct UDIFTestCase {
  enum ExpectedResults : uint16_t {
    ALL_FAIL = 0,
    UDIF_PARSE = 1 << 1,
    GET_HFS_STREAM = 1 << 2,
    READ_UDIF_DATA = 1 << 3,

    ALL_PASS = static_cast<uint16_t>(-1),  // All bits set.
  };

  // The disk image file to open.
  const char* file_name;

  base::span<const std::string_view> expected_partitions;

  // A bitmask of ExpectedResults. As the parser currently only supports
  // certain UDIF features, this is used to properly test expectations.
  int expected_results;

  // Generates a human-friendly name for the parameterized test.
  static std::string GetTestName(
      const testing::TestParamInfo<UDIFTestCase>& test) {
    std::string file = test.param.file_name;
    return file.substr(0, file.find('.'));
  }
};

class UDIFParserTest : public testing::TestWithParam<UDIFTestCase> {
 protected:
  void RunReadAllTest(size_t buffer_size) {
    const UDIFTestCase& test_case = GetParam();
    if (!(test_case.expected_results & UDIFTestCase::READ_UDIF_DATA)) {
      return;
    }

    base::File file;
    ASSERT_NO_FATAL_FAILURE(test::GetTestFile(test_case.file_name, &file));

    safe_browsing::dmg::FileReadStream file_stream(file.GetPlatformFile());
    safe_browsing::dmg::UDIFParser udif(&file_stream);
    ASSERT_TRUE(udif.Parse());

    std::vector<uint8_t> buffer(buffer_size, 0);

    for (size_t i = 0; i < udif.GetNumberOfPartitions(); ++i) {
      SCOPED_TRACE(base::StringPrintf("partition %zu", i));

      size_t total_size = udif.GetPartitionSize(i);
      size_t total_bytes_read = 0;
      std::unique_ptr<ReadStream> stream = udif.GetPartitionReadStream(i);

      bool success = false;
      do {
        size_t bytes_read = 0;
        success = stream->Read(buffer, &bytes_read);
        total_bytes_read += bytes_read;
        EXPECT_TRUE(success);
        EXPECT_TRUE(bytes_read == buffer_size ||
                    total_bytes_read == total_size)
            << "bytes_read = " << bytes_read;
      } while (total_bytes_read < total_size && success);
    }
  }
};

TEST_P(UDIFParserTest, ParseUDIF) {
  const UDIFTestCase& test_case = GetParam();

  base::File file;
  ASSERT_NO_FATAL_FAILURE(test::GetTestFile(test_case.file_name, &file));

  safe_browsing::dmg::FileReadStream file_stream(file.GetPlatformFile());
  safe_browsing::dmg::UDIFParser udif(&file_stream);

  bool expected_parse_success =
      UDIFTestCase::UDIF_PARSE & test_case.expected_results;
  ASSERT_EQ(expected_parse_success, udif.Parse());
  if (!expected_parse_success)
    return;

  EXPECT_EQ(test_case.expected_partitions.size(), udif.GetNumberOfPartitions());

  for (size_t i = 0; i < udif.GetNumberOfPartitions(); ++i) {
    SCOPED_TRACE(base::StringPrintf("partition %zu", i));
    std::unique_ptr<ReadStream> stream = udif.GetPartitionReadStream(i);

    // Apple_HFS will match both HFS and HFSX.
    if (udif.GetPartitionType(i).find("Apple_HFS") != std::string::npos) {
      ASSERT_EQ(
          (UDIFTestCase::GET_HFS_STREAM & test_case.expected_results) != 0,
          stream.get() != nullptr);
      if (!stream)
        continue;

      EXPECT_EQ(1024, stream->Seek(1024, SEEK_SET));

      HFSPlusVolumeHeader header = {0};
      bool expect_read_success =
          test_case.expected_results & UDIFTestCase::READ_UDIF_DATA;
      EXPECT_EQ(expect_read_success, stream->ReadType(header));
      if (!expect_read_success)
        continue;

      size_t size = udif.GetPartitionSize(i);
      off_t offset = stream->Seek(-1024, SEEK_END);
      ASSERT_GE(offset, 0);
      EXPECT_EQ(size - 1024, static_cast<size_t>(offset));

      HFSPlusVolumeHeader alternate_header = {0};
      EXPECT_TRUE(stream->ReadType(alternate_header));

      EXPECT_EQ(0, memcmp(&header, &alternate_header, sizeof(header)));
      EXPECT_EQ(kHFSPlusSigWord, OSSwapBigToHostInt16(header.signature));
    }

    if (test_case.expected_results & UDIFTestCase::READ_UDIF_DATA) {
      EXPECT_EQ(0, stream->Seek(0, SEEK_SET));
      size_t partition_size = udif.GetPartitionSize(i);
      std::vector<uint8_t> data(partition_size, 0);
      EXPECT_TRUE(stream->ReadExact(data));
    }
  }
}


// These tests ensure that reading the entire partition stream with different
// buffer sizes (and thus unaligned UDIF chunks) all succeed.

TEST_P(UDIFParserTest, ReadAll_8) {
  RunReadAllTest(8);
}

TEST_P(UDIFParserTest, ReadAll_512) {
  RunReadAllTest(512);
}

TEST_P(UDIFParserTest, ReadAll_1000) {
  RunReadAllTest(1000);
}

TEST_P(UDIFParserTest, ReadAll_4444) {
  RunReadAllTest(4444);
}

TEST_P(UDIFParserTest, ReadAll_8181) {
  RunReadAllTest(8181);
}

TEST_P(UDIFParserTest, ReadAll_100000) {
  RunReadAllTest(100000);
}

constexpr UDIFTestCase cases[] = {
    {"dmg_UDBZ_GPTSPUD.dmg", kGPTExpectedPartitions, UDIFTestCase::ALL_PASS},
    {"dmg_UDBZ_NONE.dmg", kNoPartitionMap, UDIFTestCase::ALL_PASS},
    {"dmg_UDBZ_SPUD.dmg", kAPMExpectedPartitions, UDIFTestCase::ALL_PASS},
    {"dmg_UDCO_GPTSPUD.dmg", kGPTExpectedPartitions,
     // ADC compression not supported.
     UDIFTestCase::UDIF_PARSE | UDIFTestCase::GET_HFS_STREAM},
    {"dmg_UDCO_NONE.dmg", kNoPartitionMap,
     // ADC compression not supported.
     UDIFTestCase::UDIF_PARSE | UDIFTestCase::GET_HFS_STREAM},
    {"dmg_UDCO_SPUD.dmg", kAPMExpectedPartitions,
     // ADC compression not supported.
     UDIFTestCase::UDIF_PARSE | UDIFTestCase::GET_HFS_STREAM},
    {"dmg_UDRO_GPTSPUD.dmg", kGPTExpectedPartitions, UDIFTestCase::ALL_PASS},
    {"dmg_UDRO_NONE.dmg", kNoPartitionMap, UDIFTestCase::ALL_PASS},
    {"dmg_UDRO_SPUD.dmg", kAPMExpectedPartitions, UDIFTestCase::ALL_PASS},
    {"dmg_UDRW_GPTSPUD.dmg", kGPTExpectedPartitions,
     // UDRW not supported.
     UDIFTestCase::ALL_FAIL},
    {"dmg_UDRW_NONE.dmg", kNoPartitionMap,
     // UDRW not supported.
     UDIFTestCase::ALL_FAIL},
    {"dmg_UDRW_SPUD.dmg", kAPMExpectedPartitions,
     // UDRW not supported.
     UDIFTestCase::ALL_FAIL},
    {"dmg_UDSP_GPTSPUD.sparseimage", kGPTExpectedPartitions,
     // Sparse images not supported.
     UDIFTestCase::ALL_FAIL},
    {"dmg_UDSP_NONE.sparseimage", kNoPartitionMap,
     // UDRW not supported.
     UDIFTestCase::ALL_FAIL},
    {"dmg_UDSP_SPUD.sparseimage", kAPMExpectedPartitions,
     // Sparse images not supported.
     UDIFTestCase::ALL_FAIL},
    {"dmg_UDTO_GPTSPUD.cdr", kGPTExpectedPartitions,
     // CD/DVD format not supported.
     UDIFTestCase::ALL_FAIL},
    {"dmg_UDTO_NONE.cdr", kNoPartitionMap,
     // CD/DVD format not supported.
     UDIFTestCase::ALL_FAIL},
    {"dmg_UDTO_SPUD.cdr", kAPMExpectedPartitions,
     // CD/DVD format not supported.
     UDIFTestCase::ALL_FAIL},
    {"dmg_UDZO_GPTSPUD.dmg", kGPTExpectedPartitions, UDIFTestCase::ALL_PASS},
    {"dmg_UDZO_SPUD.dmg", kAPMExpectedPartitions, UDIFTestCase::ALL_PASS},
    {"dmg_UFBI_GPTSPUD.dmg", kGPTExpectedPartitions, UDIFTestCase::ALL_PASS},
    {"dmg_UFBI_SPUD.dmg", kAPMExpectedPartitions, UDIFTestCase::ALL_PASS},
};

INSTANTIATE_TEST_SUITE_P(UDIFParserTest,
                         UDIFParserTest,
                         testing::ValuesIn(cases),
                         UDIFTestCase::GetTestName);

}  // namespace
}  // namespace dmg
}  // namespace safe_browsing
