// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/disk_image_type_sniffer_mac.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

struct ArchiveTestCase {
  // The disk image file to open.
  const char* file_name;

  // Expectation regarding the file being recognized as a DMG. As the UDIFParser
  // class currently only supports certain UDIF features, this is used to
  // properly test expectations.
  bool expected_results;
};

std::ostream& operator<<(std::ostream& os, const ArchiveTestCase& test_case) {
  os << test_case.file_name;
  return os;
}

class DiskImageTypeSnifferMacTest
    : public testing::TestWithParam<ArchiveTestCase> {
 protected:
  base::FilePath GetFilePath(const char* file_name) {
    base::FilePath test_data;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    return test_data.AppendASCII("safe_browsing")
        .AppendASCII("dmg")
        .AppendASCII("data")
        .AppendASCII(file_name);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_P(DiskImageTypeSnifferMacTest, SniffDiskImage) {
  const ArchiveTestCase& test_case = GetParam();
  DVLOG(1) << "Test case: " << test_case;

  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath(test_case.file_name));

  ASSERT_EQ(test_case.expected_results,
            DiskImageTypeSnifferMac::IsAppleDiskImage(path));
}

const ArchiveTestCase cases[] = {
    {"dmg_UDBZ_GPTSPUD.dmg", true},
    {"dmg_UDBZ_NONE.dmg", true},
    {"dmg_UDBZ_SPUD.dmg", true},
    {"dmg_UDCO_GPTSPUD.dmg", true},
    {"dmg_UDCO_NONE.dmg", true},
    {"dmg_UDCO_SPUD.dmg", true},
    {"dmg_UDRO_GPTSPUD.dmg", true},
    {"dmg_UDRO_NONE.dmg", true},
    {"dmg_UDRO_SPUD.dmg", true},
    // UDRW not supported.
    {"dmg_UDRW_GPTSPUD.dmg", false},
    // UDRW not supported.
    {"dmg_UDRW_NONE.dmg", false},
    // UDRW not supported.
    {"dmg_UDRW_SPUD.dmg", false},
    // Sparse images not supported.
    {"dmg_UDSP_GPTSPUD.sparseimage", false},
    // UDRW not supported.
    {"dmg_UDSP_NONE.sparseimage", false},
    // Sparse images not supported.
    {"dmg_UDSP_SPUD.sparseimage", false},
    // CD/DVD format not supported.
    {"dmg_UDTO_GPTSPUD.cdr", false},
    // CD/DVD format not supported.
    {"dmg_UDTO_NONE.cdr", false},
    // CD/DVD format not supported.
    {"dmg_UDTO_SPUD.cdr", false},
    {"dmg_UDZO_GPTSPUD.dmg", true},
    {"dmg_UDZO_SPUD.dmg", true},
    {"dmg_UFBI_GPTSPUD.dmg", true},
    {"dmg_UFBI_SPUD.dmg", true},
    {"mach_o_in_dmg.dmg", true},
    // Absence of 'koly' signature will cause parsing to fail - even if file has
    // .dmg extension.
    {"mach_o_in_dmg_no_koly_signature.dmg", false},
    // Type sniffer should realize DMG type even without extension.
    {"mach_o_in_dmg.txt", true}

};

INSTANTIATE_TEST_SUITE_P(DiskImageTypeSnifferMacTestInstantiation,
                         DiskImageTypeSnifferMacTest,
                         testing::ValuesIn(cases));

TEST(DiskImageTypeSnifferMacTest, IsAppleDiskImageTrailerIsCorrect) {
  uint8_t good_header[4] = {'k', 'o', 'l', 'y'};
  EXPECT_TRUE(DiskImageTypeSnifferMac::IsAppleDiskImageTrailer(good_header));

  uint8_t bad_header[6] = {'f', 'o', 'o', 'b', 'a', 'r'};
  EXPECT_FALSE(DiskImageTypeSnifferMac::IsAppleDiskImageTrailer(bad_header));
}

}  // namespace
}  // namespace safe_browsing
