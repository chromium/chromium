// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/chrome_utils/extension_file_logger.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/test/test_extensions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

const wchar_t kExtensionId1[] = L"extension1";
const wchar_t kTestFileName1[] = L"file1.file";

const wchar_t kExtensionId2[] = L"extension2";
const wchar_t kTestFileName2[] = L"file2.file";

const wchar_t kExtensionId3[] = L"extension3";
const wchar_t kTestFileName3[] = L"file3.file";

const wchar_t kExtensionId4[] = L"generic-extension-name";

const wchar_t kExtensionId5[] = L"ghost-extension";

}  // namespace

class ExtensionFileLoggerTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(fake_user_data_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(CreateProfileWithExtensionAndFiles(
        fake_user_data_dir_.GetPath().Append(L"Default"), kExtensionId1,
        {kTestFileName1}));

    ASSERT_TRUE(CreateProfileWithExtensionAndFiles(
        fake_user_data_dir_.GetPath().Append(L"Profile 1"), kExtensionId2,
        {kTestFileName2}));

    ASSERT_TRUE(CreateProfileWithExtensionAndFiles(
        fake_user_data_dir_.GetPath().Append(L"Profile 2"), kExtensionId3,
        {kTestFileName3}));

    ASSERT_TRUE(CreateProfileWithExtensionAndFiles(
        fake_user_data_dir_.GetPath().Append(L"Not Malicious"), kExtensionId3,
        {kTestFileName3}));

    ASSERT_TRUE(CreateProfileWithExtensionAndFiles(
        fake_user_data_dir_.GetPath().Append(L"Profile N"), kExtensionId4,
        {kTestFileName1, kTestFileName2, kTestFileName3}));

    ASSERT_TRUE(CreateProfileWithExtensionAndFiles(
        fake_user_data_dir_.GetPath().Append(L"Directory"), kExtensionId5, {}));

    base::FilePath profile_without_extensions_folder =
        fake_user_data_dir_.GetPath().Append(L"another dir");
    ASSERT_TRUE(base::CreateDirectory(profile_without_extensions_folder));

    base::FilePath folder_not_named_extensions =
        profile_without_extensions_folder.Append(L"something");
    ASSERT_TRUE(base::CreateDirectory(folder_not_named_extensions));

    base::FilePath extension_path =
        folder_not_named_extensions.Append(kExtensionId1);
    ASSERT_TRUE(base::CreateDirectory(extension_path));

    base::File extension_file(
        extension_path.Append(kTestFileName1),
        base::File::Flags::FLAG_CREATE | base::File::Flags::FLAG_READ);

    ASSERT_TRUE(extension_file.IsValid());

    file_logger_ =
        std::make_unique<ExtensionFileLogger>(fake_user_data_dir_.GetPath());
  }

 protected:
  base::ScopedTempDir fake_user_data_dir_;

  std::unique_ptr<ExtensionFileLogger> file_logger_;
};

TEST_F(ExtensionFileLoggerTest, LogExtensionOnDefaultProfile) {
  std::vector<internal::FileInformation> logged_files;

  EXPECT_TRUE(file_logger_->GetExtensionFiles(kExtensionId1, &logged_files));

  ASSERT_EQ(logged_files.size(), 1u);
  EXPECT_EQ(base::FilePath(logged_files[0].path).BaseName().value(),
            kTestFileName1);
}

TEST_F(ExtensionFileLoggerTest, LogMultipleExtensions) {
  std::vector<internal::FileInformation> logged_files;

  EXPECT_TRUE(file_logger_->GetExtensionFiles(kExtensionId1, &logged_files));
  EXPECT_TRUE(file_logger_->GetExtensionFiles(kExtensionId2, &logged_files));

  ASSERT_EQ(logged_files.size(), 2u);

  EXPECT_EQ(base::FilePath(logged_files[0].path).BaseName().value(),
            kTestFileName1);
  EXPECT_EQ(base::FilePath(logged_files[1].path).BaseName().value(),
            kTestFileName2);
}

TEST_F(ExtensionFileLoggerTest, LogExtensionOnTwoProfiles) {
  std::vector<internal::FileInformation> logged_files;

  EXPECT_TRUE(file_logger_->GetExtensionFiles(kExtensionId3, &logged_files));

  ASSERT_EQ(logged_files.size(), 2u);

  EXPECT_EQ(base::FilePath(logged_files[0].path).BaseName().value(),
            kTestFileName3);
  EXPECT_EQ(base::FilePath(logged_files[1].path).BaseName().value(),
            kTestFileName3);
}

TEST_F(ExtensionFileLoggerTest, DoNotLogNonExistingExtension) {
  std::vector<internal::FileInformation> logged_files;

  EXPECT_FALSE(
      file_logger_->GetExtensionFiles(L"RandomExtension", &logged_files));
}

TEST_F(ExtensionFileLoggerTest, LogExtensionWithMultipleFiles) {
  std::vector<internal::FileInformation> logged_files;

  EXPECT_TRUE(file_logger_->GetExtensionFiles(kExtensionId4, &logged_files));

  ASSERT_EQ(logged_files.size(), 3u);

  std::vector<std::wstring> returned_paths = {
      base::FilePath(logged_files[0].path).BaseName().value(),
      base::FilePath(logged_files[1].path).BaseName().value(),
      base::FilePath(logged_files[2].path).BaseName().value()};
  EXPECT_THAT(returned_paths,
              testing::UnorderedElementsAreArray(
                  {kTestFileName1, kTestFileName2, kTestFileName3}));
}

TEST_F(ExtensionFileLoggerTest, LogExtensionWithoutFiles) {
  std::vector<internal::FileInformation> logged_files;
  EXPECT_TRUE(file_logger_->GetExtensionFiles(kExtensionId5, &logged_files));
  EXPECT_EQ(logged_files.size(), 0u);
}

}  // namespace chrome_cleaner
