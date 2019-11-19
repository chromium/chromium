// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ui/silent_main_dialog.h"

#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_settings_util.h"
#include "chrome/chrome_cleaner/ui/mock_main_dialog_delegate.h"
#include "components/chrome_cleaner/test/test_name_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {
namespace {

class SilentMainDialogTest : public ::testing::Test {
 public:
  SilentMainDialogTest() {
    dialog_ = std::make_unique<SilentMainDialog>(&delegate_);
  }

 protected:
  ::testing::StrictMock<MockMainDialogDelegate> delegate_;
  std::unique_ptr<SilentMainDialog> dialog_;
};

TEST_F(SilentMainDialogTest, NoPUPsFound) {
  EXPECT_CALL(delegate_, OnClose()).Times(1);
  dialog_->NoPUPsFound();
}

TEST_F(SilentMainDialogTest, CleanupDone) {
  EXPECT_CALL(delegate_, OnClose()).Times(1);
  dialog_->CleanupDone(RESULT_CODE_SUCCESS);
}

TEST_F(SilentMainDialogTest, Close) {
  EXPECT_CALL(delegate_, OnClose()).Times(1);
  dialog_->Close();
}

enum class FileTypeToTest {
  kNone,
  kExecutable,
  kText,
};

std::ostream& operator<<(std::ostream& stream,
                         FileTypeToTest file_type_to_test) {
  switch (file_type_to_test) {
    case FileTypeToTest::kExecutable:
      stream << "ExecutableFile";
      break;
    case FileTypeToTest::kText:
      stream << "TextFile";
      break;
    case FileTypeToTest::kNone:
      stream << "NoFile";
      break;
  }
  return stream;
}

class ConfirmCleanupSilentMainDialogTest
    : public SilentMainDialogTest,
      public ::testing::WithParamInterface<FileTypeToTest> {
 public:
  ConfirmCleanupSilentMainDialogTest() {
    Settings::SetInstanceForTesting(&mock_settings_);
  }

  ~ConfirmCleanupSilentMainDialogTest() override {
    Settings::SetInstanceForTesting(nullptr);
  }

 protected:
  ::testing::NiceMock<MockSettings> mock_settings_;
};

TEST_P(ConfirmCleanupSilentMainDialogTest, ConfirmCleanup) {
  constexpr UwSId kFakePupId = 1024;
  FileTypeToTest file_type_to_test = GetParam();

  // Add a PUP and some disk footprints.
  TestPUPData test_pup_data;
  test_pup_data.AddPUP(kFakePupId, PUPData::FLAGS_ACTION_REMOVE, "",
                       PUPData::kMaxFilesToRemoveSmallUwS);
  PUPData::PUP* pup = PUPData::GetPUP(kFakePupId);
  switch (file_type_to_test) {
    case FileTypeToTest::kExecutable:
      ASSERT_TRUE(pup->AddDiskFootprint(
          base::FilePath(FILE_PATH_LITERAL("c:\\file.exe"))));
      EXPECT_CALL(delegate_, AcceptedCleanup(true)).Times(1);
      break;
    case FileTypeToTest::kText:
      ASSERT_TRUE(pup->AddDiskFootprint(
          base::FilePath(FILE_PATH_LITERAL("c:\\file.txt"))));
      // Even if only a non-executable file is found, the user should be
      // prompted.
      EXPECT_CALL(delegate_, AcceptedCleanup(true)).Times(1);
      break;
    case FileTypeToTest::kNone:
      EXPECT_CALL(delegate_, OnClose()).Times(1);
      break;
  }

  std::vector<UwSId> found_pups{kFakePupId};
  dialog_->ConfirmCleanupIfNeeded(found_pups, nullptr);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ConfirmCleanupSilentMainDialogTest,
                         ::testing::Values(FileTypeToTest::kNone,
                                           FileTypeToTest::kExecutable,
                                           FileTypeToTest::kText),
                         GetParamNameForTest());

}  // namespace
}  // namespace chrome_cleaner
