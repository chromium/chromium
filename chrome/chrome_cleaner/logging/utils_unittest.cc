// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/utils.h"

#include <shlobj.h>
#include <map>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "chrome/chrome_cleaner/logging/info_sampler.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

TEST(DiskUtilTests, RetrieveFolderInformation) {
  base::ScopedPathOverride appdata_override(
      CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA));
  base::FilePath appdata_folder;

  ASSERT_TRUE(base::PathService::Get(CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA),
                                     &appdata_folder));
  base::FilePath temp_folder;
  base::CreateTemporaryDirInDir(appdata_folder, L"temp_folder123",
                                &temp_folder);

  FolderInformation folder_information;
  EXPECT_TRUE(RetrieveFolderInformation(temp_folder, &folder_information));

  // The expected file path value should be sanitized.
  EXPECT_EQ(base::WideToUTF8(SanitizePath(temp_folder)),
            folder_information.path());
  EXPECT_FALSE(folder_information.creation_date().empty());
  EXPECT_FALSE(folder_information.last_modified_date().empty());
}

TEST(DiskUtilTests, RetrieveFolderInformationNoFile) {
  base::ScopedPathOverride appdata_override(
      CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA));
  base::FilePath appdata_folder;
  ASSERT_TRUE(base::PathService::Get(CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA),
                                     &appdata_folder));

  base::FilePath non_existent_folder(
      appdata_folder.DirName().Append(L"non-existent-folder"));

  FolderInformation folder_information;
  EXPECT_FALSE(
      RetrieveFolderInformation(non_existent_folder, &folder_information));

  EXPECT_TRUE(folder_information.path().empty());
  EXPECT_TRUE(folder_information.creation_date().empty());
  EXPECT_TRUE(folder_information.last_modified_date().empty());
}

// Returns all files in |files| that start with a fixed pattern.
class PrefixInfoSampler : public InfoSampler {
 public:
  explicit PrefixInfoSampler(const std::wstring& prefix) : prefix_(prefix) {}

  void SelectPathSetToSample(const FilePathSet& files,
                             FilePathSet* sampled_file_paths) override {
    for (const base::FilePath& file : files.file_paths()) {
      if (base::StartsWith(file.BaseName().value(), prefix_,
                           base::CompareCase::INSENSITIVE_ASCII)) {
        sampled_file_paths->Insert(file);
      }
    }
  }

 private:
  const std::wstring prefix_;
};

class PUPToUwSTest : public ::testing::TestWithParam<bool> {
 public:
  static constexpr UwSId kTestPUPId = 12321;

  enum class FileCategory {
    kInactive,  // File should be inactive based on the file name.
    kActive,    // File should be active based on the file name.
  };

  struct FileFlags {
    FileCategory category;
    bool has_details;
  };

  PUPToUwSTest() : is_cleaning_(GetParam()), sampler_(L"sampled_") {
    expectations_ = {
        // If cleaning, active files and sampled files should have details.
        // Otherwise only sampled files should have details.
        {L"active_file.exe", {FileCategory::kActive, is_cleaning_}},
        {L"inactive_file.txt", {FileCategory::kInactive, false}},
        // Only files beginning with "sampled_" will be chosen by the
        // sampler.
        {L"sampled_active_file.exe", {FileCategory::kActive, true}},
        {L"sampled_inactive_file.txt", {FileCategory::kInactive, true}},
    };
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Create a PUPData entry that would match the expected files.
    test_pup_data_.AddPUP(kTestPUPId, PUPData::FLAGS_NONE, nullptr,
                          PUPData::kMaxFilesToRemoveSmallUwS);
    for (const auto& expectation : expectations_) {
      const std::wstring filename = expectation.first;
      test_pup_data_.AddDiskFootprint(kTestPUPId, CSIDL_STARTUP,
                                      filename.c_str(),
                                      PUPData::DISK_MATCH_ANY_FILE);

      // Files can't be matched unless they exist on disk.
      ASSERT_TRUE(CreateFileInFolder(temp_dir_.GetPath(), filename.c_str()));
    }
  }

  const PUPData::PUP* GetPUPWithExpectedFiles() const {
    // Create a found PUP that matched the files.
    PUPData::PUP* pup = PUPData::GetPUP(kTestPUPId);
    DCHECK(pup);
    for (const auto& expectation : expectations_) {
      const std::wstring filename = expectation.first;
      pup->AddDiskFootprint(temp_dir_.GetPath().Append(filename));
    }
    return pup;
  }

 protected:
  bool is_cleaning_;

  PrefixInfoSampler sampler_;
  std::map<std::wstring, FileFlags> expectations_;
  base::ScopedTempDir temp_dir_;
  TestPUPData test_pup_data_;
};

INSTANTIATE_TEST_SUITE_P(PUPToUwS,
                         PUPToUwSTest,
                         ::testing::Values(false, true));

TEST_P(PUPToUwSTest, PUPToUwS) {
  UwS uws = PUPToUwS(GetPUPWithExpectedFiles(), kUwSDetectedFlagsNone,
                     is_cleaning_, &sampler_);

  // Loop through all converted files and make sure each was converted
  // correctly.
  std::vector<std::wstring> converted_files;
  for (int i = 0; i < uws.files_size(); ++i) {
    ASSERT_TRUE(uws.files(i).has_file_information());
    const FileInformation& file_info = uws.files(i).file_information();
    ASSERT_TRUE(file_info.has_path());
    base::FilePath file_path(base::UTF8ToWide(file_info.path()));
    std::wstring uws_filename =
        base::ToLowerASCII(file_path.BaseName().value());
    converted_files.push_back(uws_filename);

    SCOPED_TRACE(uws_filename);

    auto expectation = expectations_.find(uws_filename);
    ASSERT_FALSE(expectation == expectations_.end());
    EXPECT_EQ(expectation->second.category != FileCategory::kInactive,
              file_info.active_file());
    EXPECT_EQ(expectation->second.has_details, file_info.has_sha256());
  }

  std::vector<std::wstring> expected_files;
  for (const auto& expectation : expectations_)
    expected_files.push_back(expectation.first);
  EXPECT_THAT(converted_files,
              ::testing::UnorderedElementsAreArray(expected_files));

  EXPECT_NE(Engine::UNKNOWN, uws.detected_by());
}

}  // namespace chrome_cleaner
