// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/logging_conversion.h"

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "chrome/chrome_cleaner/engines/common/engine_result_codes.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "chrome/chrome_cleaner/test/test_uws_catalog.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

const unsigned int kScanOnlyUwSId = kGoogleTestAUwSID;
const unsigned int kCleanableUwSId = kGoogleTestBUwSID;
const unsigned int kInvalidUwSId = 42;

class EngineLoggingConversionTest : public ::testing::Test {
 protected:
  EngineLoggingConversionTest() {
    test_pup_data_.Reset({&TestUwSCatalog::GetInstance()});
  }

  TestPUPData test_pup_data_;
};

TEST_F(EngineLoggingConversionTest, UwSLoggingDecision) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath existing1 = temp_dir.GetPath().Append(L"existing1.exe");
  ASSERT_TRUE(CreateEmptyFile(existing1));
  base::FilePath existing2 = temp_dir.GetPath().Append(L"existing2.exe");
  ASSERT_TRUE(CreateEmptyFile(existing2));
  base::FilePath non_existing = temp_dir.GetPath().Append(L"non_existing.exe");

  // Unsupported UwS.
  EXPECT_EQ(LoggingDecision::kUnsupported, UwSLoggingDecision(kInvalidUwSId));

  // Scan only UwS.
  EXPECT_EQ(LoggingDecision::kNotLogged, UwSLoggingDecision(kScanOnlyUwSId));

  PUPData::PUP* pup1 = PUPData::GetPUP(kScanOnlyUwSId);
  // Non-existing file will be removed from the list and this UwS won't be
  // logged.
  pup1->AddDiskFootprint(non_existing);
  EXPECT_EQ(LoggingDecision::kNotLogged, UwSLoggingDecision(kScanOnlyUwSId));

  // Logged because of existing files.
  pup1->AddDiskFootprint(existing1);
  pup1->AddDiskFootprint(existing2);
  pup1->AddDiskFootprint(non_existing);
  EXPECT_EQ(LoggingDecision::kLogged, UwSLoggingDecision(kScanOnlyUwSId));

  // Cleanable UwS.
  PUPData::PUP* pup2 = PUPData::GetPUP(kCleanableUwSId);
  // Non-existing file will be removed from the list and this UwS won't be
  // logged.
  pup2->AddDiskFootprint(non_existing);
  EXPECT_EQ(LoggingDecision::kNotLogged, UwSLoggingDecision(kCleanableUwSId));

  // Logged because of existing file.
  pup2->AddDiskFootprint(non_existing);
  pup2->AddDiskFootprint(existing1);
  EXPECT_EQ(LoggingDecision::kLogged, UwSLoggingDecision(kCleanableUwSId));
}

TEST_F(EngineLoggingConversionTest, ScanningResultCode) {
  EXPECT_EQ(RESULT_CODE_CANCELED,
            ScanningResultCode(EngineResultCode::kCancelled));

  EXPECT_EQ(RESULT_CODE_SCANNING_ENGINE_ERROR,
            ScanningResultCode(EngineResultCode::kWrongState));

  EXPECT_EQ(RESULT_CODE_SUCCESS,
            ScanningResultCode(EngineResultCode::kSuccess));
}

TEST_F(EngineLoggingConversionTest, CleaningResultCode) {
  EXPECT_EQ(RESULT_CODE_CANCELED,
            CleaningResultCode(EngineResultCode::kCancelled,
                               /*needs_reboot=*/false));
  EXPECT_EQ(RESULT_CODE_CANCELED,
            CleaningResultCode(EngineResultCode::kCancelled,
                               /*needs_reboot=*/true));
  EXPECT_EQ(RESULT_CODE_FAILED,
            CleaningResultCode(EngineResultCode::kCleaningFailed,
                               /*needs_reboot=*/false));
  EXPECT_EQ(RESULT_CODE_FAILED,
            CleaningResultCode(EngineResultCode::kCleaningFailed,
                               /*needs_reboot=*/true));
  EXPECT_EQ(RESULT_CODE_FAILED,
            CleaningResultCode(EngineResultCode::kEngineInternal,
                               /*needs_reboot=*/false));
  EXPECT_EQ(RESULT_CODE_FAILED,
            CleaningResultCode(EngineResultCode::kEngineInternal,
                               /*needs_reboot=*/true));

  EXPECT_EQ(RESULT_CODE_SUCCESS, CleaningResultCode(EngineResultCode::kSuccess,
                                                    /*needs_reboot=*/false));
  EXPECT_EQ(RESULT_CODE_PENDING_REBOOT,
            CleaningResultCode(EngineResultCode::kSuccess,
                               /*needs_reboot=*/true));
}

}  // namespace

}  // namespace chrome_cleaner
