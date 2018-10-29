// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/scanner/urza_scanner_impl.h"

#include <shlobj.h>

#include <algorithm>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_path_override.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_shortcut_win.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/mock_logging_service.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/logging/utils.h"
#include "chrome/chrome_cleaner/os/disk_util.h"
#include "chrome/chrome_cleaner/proto/shared_pup_enums.pb.h"
#include "chrome/chrome_cleaner/strings/string_util.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_name_helper.h"
#include "chrome/chrome_cleaner/test/test_pup_data.h"
#include "chrome/chrome_cleaner/test/test_registry_util.h"
#include "chrome/chrome_cleaner/test/test_signature_matcher.h"
#include "chrome/chrome_cleaner/test/test_strings.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

using testing::_;
using testing::Eq;

// To make git cl lint happy.
constexpr size_t kMaxPath = _MAX_PATH;

constexpr UwSId k12ID = 12;
constexpr UwSId k24ID = 24;
constexpr UwSId k42ID = 42;
constexpr UwSId k84ID = 84;

constexpr UwSId kNonExistentID = 666;

constexpr base::char16 kBaseName[] = L"foo";
constexpr base::char16 kOtherBaseName[] = L"goo";
constexpr base::char16 kBadBaseName[] = L"fo";
constexpr base::char16 kValueName[] = L"bar";
constexpr base::char16 kValue[] = L"bla";
constexpr base::char16 kValueDifferentCase[] = L"bLa";
constexpr base::char16 kBadValue[] = L"bl";
constexpr base::char16 kBaValue[] = L"ba";
constexpr base::char16 kFooValue[] = L"foo";
constexpr base::char16 kComplexValue[] = L"bli bla blue";
constexpr base::char16 kCommaSetValue[] = L"bli,bla,blue";
constexpr base::char16 kCommonSetValue[] = L"bli,bla blue";
constexpr base::char16 kCommonSetWithNullValue[] = L"bli\0bla\0blue";
constexpr base::char16 kBiggerCommaSetValue[] = L"bli,blablabla,blue";
constexpr base::char16 kBadCommaSetValue[] = L"bli,bah,blue";
constexpr base::char16 kWildcardSearch1[] = L"*";
constexpr base::char16 kWildcardSearch2[] = L"f??";
constexpr base::char16 kDummyFullPath[] = L"c:\\foo\\bar\\bat.exe";
constexpr base::char16 kOtherFullPath[] = L"c:\\foo\\bar.exe";
constexpr base::char16 kLongFileName[] = L"bla bla bla.exe";

constexpr base::char16 kRegistryKeyPath[] = L"software\\foo";
constexpr base::char16 kValueName1[] = L"foo1";
constexpr base::char16 kValueName2[] = L"foo2";
constexpr base::char16 kValueName3[] = L"foo3";
constexpr base::char16 kValueNameSingleWildcard[] = L"foo?";
constexpr base::char16 kValueNameMultiWildcard[] = L"fo*";
constexpr base::char16 kSetWithCommonValue1[] = L"foo bar bat";
constexpr base::char16 kSetWithCommonValue2[] = L"bar foo bat";
constexpr base::char16 kSetWithCommonValue3[] = L"bar bat foo";

constexpr size_t kManyPUPs = 1000;
constexpr size_t kSomePUPs = 20;
constexpr size_t kFoundPUPsModuloBase = 10;

bool g_test_custom_matcher1_called = false;
bool g_test_custom_matcher2_called = false;
bool g_test_custom_matcher3_called = false;
bool g_test_custom_matcher5_found = false;

// Computes the set difference between two sets and outputs to a vector.
template <typename T>
void set_difference(const std::set<T>& set1,
                    const std::set<T>& set2,
                    std::vector<T>* output) {
  std::set_difference(set1.begin(), set1.end(), set2.begin(), set2.end(),
                      std::back_inserter(*output));
}

class TestScanner : public UrzaScannerImpl {
 public:
  TestScanner() = default;
  explicit TestScanner(const MatchingOptions& options,
                       SignatureMatcherAPI* signature_matcher,
                       RegistryLogger* registry_logger)
      : UrzaScannerImpl(options, signature_matcher, registry_logger) {}
  ~TestScanner() override = default;
};

// This test fixture is used to expose callbacks to the scanner and some utility
// functions. It keeps data for the found PUPs, as well as the done and stopped
// state. It also creates the main/UI task scheduler.
class ScannerTest : public testing::Test {
 public:
  ScannerTest()
      : done_scanning_(false),
        found_report_only_(false),
        found_pup_to_remove_(false),
        stopped_(false),
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

  void TearDown() override {
    // Even though not all tests override the logging instance, we reset it
    // during test tear down, so test failures don't keep them in an invalid
    // status.
    LoggingServiceAPI::SetInstanceForTesting(nullptr);
  }

  // Scanner callbacks.
  void DoneScanning(ResultCode status, const std::vector<UwSId>& found_pups) {
    ASSERT_FALSE(done_scanning_);
    found_pups_.insert(found_pups_.begin(), found_pups.begin(),
                       found_pups.end());
    done_scanning_ = true;

    // Urza is so cool, it never fails.
    EXPECT_EQ(RESULT_CODE_SUCCESS, status);

    found_report_only_ =
        PUPData::HasFlaggedPUP(found_pups, &PUPData::HasReportOnlyFlag);
  }

  void FoundUwS(UwSId found_pup) {
    EXPECT_FALSE(done_scanning_);
    EXPECT_EQ(pups_seen_in_progress_callback_.end(),
              pups_seen_in_progress_callback_.find(found_pup));
    pups_seen_in_progress_callback_.insert(found_pup);
  }

  // Start/Stop the scanner.
  void Scan() {
    done_scanning_ = false;
    g_test_custom_matcher1_called = false;
    g_test_custom_matcher2_called = false;
    g_test_custom_matcher3_called = false;
    g_test_custom_matcher5_found = false;
    found_pups_.clear();
    pups_seen_in_progress_callback_.clear();

    test_scanner_.reset(
        new TestScanner(options_, &test_signature_matcher_, nullptr));

    test_scanner_->Start(
        base::BindRepeating(&ScannerTest::FoundUwS, base::Unretained(this)),
        base::BindOnce(&ScannerTest::DoneScanning, base::Unretained(this)));
    // Now wait for the scanner to be done or stopped.
    while (!(done_scanning_ || stopped_) || !test_scanner_->IsCompletelyDone())
      base::RunLoop().RunUntilIdle();
  }

  void StopScanner() {
    test_scanner_->Stop();
    stopped_ = true;
  }

  // Expectations utility functions.
  void ExpectNoPUPsFound() { ExpectFoundPUPs({}); }

  void ExpectSinglePUP(UwSId pup_id) { ExpectFoundPUPs({pup_id}); }

  void ExpectFoundPUPs(const std::set<UwSId>& pups) {
    EXPECT_EQ(pups.size(), found_pups_.size());
    std::set<UwSId>::const_iterator pup = pups.begin();
    for (; pup != pups.end(); ++pup)
      EXPECT_TRUE(base::ContainsValue(found_pups_, *pup));
    EXPECT_EQ(pups.size(), pups_seen_in_progress_callback_.size());
    EXPECT_EQ(pups, pups_seen_in_progress_callback_);
  }

  // Add disk footprint for PUP |pup_id| to |test_pup_data|. If
  // |disk_footprint_exist| is true, create files matching the newly created
  // footprint.
  // Return true on success.
  bool SetupDiskFootprint(TestPUPData* test_pup_data,
                          UwSId pup_id,
                          bool disk_footprint_exist) {
    // Create temporary directory.
    std::shared_ptr<base::ScopedTempDir> temp_dir(new base::ScopedTempDir);
    if (!temp_dir->CreateUniqueTempDir() || !temp_dir->IsValid())
      return false;

    // Setup disk footprint rule. PUPData holds C-style strings, so provide
    // C string equivalent of the stored filepath.
    pup_data_filepaths_.push_back(temp_dir->GetPath().value());
    test_pup_data->AddDiskFootprint(pup_id, 0,
                                    pup_data_filepaths_.back().c_str(),
                                    PUPData::DISK_MATCH_ANY_FILE);

    // If footprint rule should match, create a file and ensure the directory is
    // not deleted until the end of the test.
    if (disk_footprint_exist) {
      base::FilePath temp_file;
      if (!CreateTemporaryFileInDir(temp_dir->GetPath(), &temp_file))
        return false;
      pup_data_temp_dirs_.push_back(temp_dir);
    }

    return true;
  }

 protected:
  std::unique_ptr<TestScanner> test_scanner_;
  MatchingOptions options_;
  TestSignatureMatcher test_signature_matcher_;
  std::vector<UwSId> found_pups_;
  std::set<UwSId> pups_seen_in_progress_callback_;
  bool done_scanning_;
  bool found_report_only_;
  bool found_pup_to_remove_;
  bool stopped_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // This vector holds temporary directories which should be released at the end
  // of the test.
  std::vector<std::shared_ptr<base::ScopedTempDir>> pup_data_temp_dirs_;
  // This vector holds C++ strings representing PUP disk footprints filepaths.
  // This is to allow proper memory management of C string equivalents in
  // PUPData.
  std::vector<base::string16> pup_data_filepaths_;
};

class ScannerTestWithBitness : public ScannerTest,
                               public ::testing::WithParamInterface<int> {};

INSTANTIATE_TEST_CASE_P(ScannerBitnessTest,
                        ScannerTestWithBitness,
                        testing::Values(32, 64),
                        GetParamNameForTest());

// A custom matcher finding an active disk footprint.
bool TestCustomMatcher1(const MatchingOptions& options,
                        const SignatureMatcherAPI* signature_matcher,
                        PUPData::PUP* pup,
                        bool* active_footprint_found) {
  DCHECK(pup);
  DCHECK(active_footprint_found);
  base::FilePath path(kDummyFullPath);
  pup->AddDiskFootprint(path);
  *active_footprint_found = true;
  g_test_custom_matcher1_called = true;
  return true;
}

// A custom matcher finding an inactive (left-over) disk footprint.
bool TestCustomMatcher2(const MatchingOptions& options,
                        const SignatureMatcherAPI* signature_matcher,
                        PUPData::PUP* pup,
                        bool* active_footprint_found) {
  DCHECK(pup);
  DCHECK(active_footprint_found);
  base::FilePath path(kOtherFullPath);
  pup->AddDiskFootprint(path);
  g_test_custom_matcher2_called = true;
  return true;
}

// A custom matcher finding nothing.
bool TestCustomMatcher3(const MatchingOptions& options,
                        const SignatureMatcherAPI* signature_matcher,
                        PUPData::PUP* pup,
                        bool* active_footprint_found) {
  DCHECK(pup);
  DCHECK(active_footprint_found);
  g_test_custom_matcher3_called = true;
  return true;
}

// A custom matcher producing an error.
bool TestCustomMatcher4(const MatchingOptions& options,
                        const SignatureMatcherAPI* signature_matcher,
                        PUPData::PUP* pup,
                        bool* active_footprint_found) {
  DCHECK(pup);
  DCHECK(active_footprint_found);
  base::FilePath path(kDummyFullPath);
  pup->AddDiskFootprint(path);
  *active_footprint_found = true;
  // Return an error, abort scanning this pup.
  return false;
}

// A custom matcher setting |g_test_custom_matcher5_found| to
// |active_footprint_found|.
bool TestCustomMatcher5(const MatchingOptions& options,
                        const SignatureMatcherAPI* signature_matcher,
                        PUPData::PUP* pup,
                        bool* active_footprint_found) {
  DCHECK(pup);
  DCHECK(active_footprint_found);
  g_test_custom_matcher5_found = *active_footprint_found;
  return true;
}

}  // namespace

TEST_F(ScannerTest, NonexistentDiskFootprint) {
  // Set up a disk footprint.
  base::ScopedTempDir scoped_temp_dir1;
  ASSERT_TRUE(scoped_temp_dir1.CreateUniqueTempDir());
  // Only the folder was actually created, so base name is nonexistent.
  base::FilePath nonexistent_file(scoped_temp_dir1.GetPath().Append(kBaseName));

  TestPUPData test_pup_data;
  test_pup_data.AddDiskFootprint(k42ID, 0, nonexistent_file.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);

  Scan();
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, NonexistentRelativeDiskFootprint) {
  base::char16 special_folder_path[kMaxPath];
  HRESULT hr = SHGetFolderPath(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
                               SHGFP_TYPE_CURRENT, special_folder_path);
  ASSERT_EQ(S_OK, hr);

  base::ScopedTempDir scoped_temp_dir2;
  ASSERT_TRUE(scoped_temp_dir2.CreateUniqueTempDirUnderPath(
      base::FilePath(special_folder_path)));
  base::FilePath nonexistent_footprint(
      scoped_temp_dir2.GetPath().BaseName().Append(kBaseName));
  TestPUPData test_pup_data;
  test_pup_data.AddDiskFootprint(k42ID, CSIDL_LOCAL_APPDATA,
                                 nonexistent_footprint.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);
  ASSERT_FALSE(base::PathExists(scoped_temp_dir2.GetPath().Append(kBaseName)));

  Scan();
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, NonexistentWildcardDiskFootprint) {
  // Set up a disk footprint.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath existent_folder(scoped_temp_dir.GetPath());
  // Only the folder was actually created, so base name is nonexistent.
  base::FilePath nonexistent_file(
      scoped_temp_dir.GetPath().Append(kWildcardSearch2));

  TestPUPData test_pup_data;
  test_pup_data.AddDiskFootprint(k42ID, 0, nonexistent_file.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);

  // File should not be found because it doesn't match the pattern.
  base::FilePath temp_file;
  ASSERT_TRUE(CreateTemporaryFileInDir(existent_folder, &temp_file));

  Scan();
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, NonexistentRegistryFootprint) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Make sure the key doesn't exist.
  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_FILE_NOT_FOUND,
            system_registry_key.Open(HKEY_LOCAL_MACHINE, kBaseName, KEY_READ));
  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, nullptr, nullptr,
                                     REGISTRY_VALUE_MATCH_KEY);

  Scan();
  ExpectNoPUPsFound();

  // Now create the key, so that we can look for a nonexistent value.
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));

  test_pup_data.Reset({});
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, kValueName, nullptr,
                                     REGISTRY_VALUE_MATCH_VALUE_NAME);
  Scan();
  ExpectNoPUPsFound();

  // Now create the value, but with a bad name while we look for the good one.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kBadValue));

  test_pup_data.Reset({});
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, kValueName, kValue,
                                     REGISTRY_VALUE_MATCH_PARTIAL);
  Scan();
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, ExistentDiskFootprint) {
  // Set up a folder only disk footprint.
  base::ScopedTempDir scoped_temp_dir1;
  ASSERT_TRUE(scoped_temp_dir1.CreateUniqueTempDir());
  base::FilePath existent_folder(scoped_temp_dir1.GetPath());
  ASSERT_TRUE(base::PathExists(existent_folder));

  base::ScopedTempDir scoped_temp_dir2;
  ASSERT_TRUE(scoped_temp_dir2.CreateUniqueTempDirUnderPath(
      scoped_temp_dir1.GetPath()));
  base::FilePath existent_subfolder(scoped_temp_dir2.GetPath());
  ASSERT_TRUE(base::PathExists(existent_subfolder));

  TestPUPData test_pup_data;
  test_pup_data.AddDiskFootprint(k42ID, 0, existent_folder.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);
  const PUPData::PUP* found_pup = PUPData::GetPUP(k42ID);

  // No PUP should be found when only folders are under the marked footprints.
  Scan();
  ExpectNoPUPsFound();

  // Add a file, and it should be found.
  base::FilePath temp_file;
  ASSERT_TRUE(CreateTemporaryFileInDir(existent_subfolder, &temp_file));

  Scan();
  ExpectSinglePUP(k42ID);

  EXPECT_EQ(3UL, found_pup->expanded_disk_footprints.size());
  ExpectDiskFootprint(*found_pup, existent_folder);
  ExpectDiskFootprint(*found_pup, existent_subfolder);
  ExpectDiskFootprint(*found_pup, temp_file);

  ASSERT_TRUE(base::DeleteFile(temp_file, false));
  Scan();
  ExpectNoPUPsFound();

  // Add a file deeper in a folder hierarchy, and it should still be found.
  base::FilePath temp_folder;
  ASSERT_TRUE(CreateTemporaryDirInDir(existent_subfolder, L"", &temp_folder));
  ASSERT_TRUE(CreateTemporaryFileInDir(temp_folder, &temp_file));

  Scan();
  ExpectSinglePUP(k42ID);
  EXPECT_EQ(4UL, found_pup->expanded_disk_footprints.size());
  ExpectDiskFootprint(*found_pup, existent_folder);
  ExpectDiskFootprint(*found_pup, existent_subfolder);
  ExpectDiskFootprint(*found_pup, temp_folder);
  ExpectDiskFootprint(*found_pup, temp_file);
}

TEST_F(ScannerTest, ExistentRelativeDiskFootprint) {
  // Set up a relative disk footprint.
  base::char16 special_folder_path[kMaxPath];
  HRESULT hr = SHGetFolderPath(nullptr, CSIDL_PROGRAM_FILES, nullptr,
                               SHGFP_TYPE_CURRENT, special_folder_path);
  ASSERT_EQ(S_OK, hr);

  base::ScopedTempDir scoped_temp_dir3;
  ASSERT_TRUE(scoped_temp_dir3.CreateUniqueTempDirUnderPath(
      base::FilePath(special_folder_path)));
  base::FilePath existent_relative_footprint(
      scoped_temp_dir3.GetPath().BaseName());
  TestPUPData test_pup_data;
  test_pup_data.AddDiskFootprint(k24ID, CSIDL_PROGRAM_FILES,
                                 existent_relative_footprint.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);
  const PUPData::PUP* found_pup = PUPData::GetPUP(k24ID);

  // Empty folder should not be found.
  Scan();
  ExpectNoPUPsFound();

  // Now create a file in it, so it can be found.
  base::FilePath temp_file;
  ASSERT_TRUE(CreateTemporaryFileInDir(scoped_temp_dir3.GetPath(), &temp_file));

  Scan();
  ExpectSinglePUP(k24ID);
  EXPECT_EQ(2UL, found_pup->expanded_disk_footprints.size());
  ExpectDiskFootprint(*found_pup, scoped_temp_dir3.GetPath());
  ExpectDiskFootprint(*found_pup, temp_file);
}

TEST_P(ScannerTestWithBitness, ProgramFilesDiskFootprint) {
  ASSERT_TRUE(GetParam() == 32 || GetParam() == 64);

  base::FilePath program_files;
  if (GetParam() == 32) {
    program_files = GetX86ProgramFilesPath(base::FilePath());
  } else {
    program_files = GetX64ProgramFilesPath(base::FilePath());
  }

  if (program_files.empty()) {
    ASSERT_EQ(64, GetParam());
    ASSERT_EQ(base::win::OSInfo::X86_ARCHITECTURE,
              base::win::OSInfo::GetInstance()->architecture());
    return;
  }

  base::ScopedTempDir scoped_temp_dir3;
  ASSERT_TRUE(scoped_temp_dir3.CreateUniqueTempDirUnderPath(program_files));
  base::FilePath existent_relative_footprint(
      scoped_temp_dir3.GetPath().BaseName());
  TestPUPData test_pup_data;
  test_pup_data.AddDiskFootprint(k24ID, CSIDL_PROGRAM_FILES,
                                 existent_relative_footprint.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);
  const PUPData::PUP* found_pup = PUPData::GetPUP(k24ID);

  // Empty folder should not be found.
  Scan();
  ExpectNoPUPsFound();

  // Now create a file in it, so it can be found.
  base::FilePath temp_file;
  ASSERT_TRUE(CreateTemporaryFileInDir(scoped_temp_dir3.GetPath(), &temp_file));

  Scan();
  ExpectSinglePUP(k24ID);
  EXPECT_EQ(2UL, found_pup->expanded_disk_footprints.size());
  ExpectDiskFootprint(*found_pup, scoped_temp_dir3.GetPath());
  ExpectDiskFootprint(*found_pup, temp_file);
}

TEST_F(ScannerTest, UnicodeDiskFootprint) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath existent_folder(scoped_temp_dir.GetPath());
  ASSERT_TRUE(base::PathExists(existent_folder));
  base::FilePath sub_folder(scoped_temp_dir.GetPath().Append(kValidUtf8Name));
  ASSERT_TRUE(base::CreateDirectory(sub_folder));

  TestPUPData test_pup_data;
  test_pup_data.AddDiskFootprint(k42ID, 0, sub_folder.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);
  const PUPData::PUP* found_pup = PUPData::GetPUP(k42ID);

  // Now create a file in it, so it can be found.
  base::FilePath temp_file;
  ASSERT_TRUE(CreateTemporaryFileInDir(sub_folder, &temp_file));

  Scan();
  ExpectSinglePUP(k42ID);
  EXPECT_EQ(2UL, found_pup->expanded_disk_footprints.size());
  ExpectDiskFootprint(*found_pup, sub_folder);
  ExpectDiskFootprint(*found_pup, temp_file);
}

TEST_F(ScannerTest, ExistentWildcardDiskFootprint) {
  // Set up a folder only disk footprint.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath existent_folder(scoped_temp_dir.GetPath());
  ASSERT_TRUE(base::PathExists(existent_folder));

  base::FilePath existent_file(
      scoped_temp_dir.GetPath().Append(kWildcardSearch1));
  TestPUPData test_pup_data;
  test_pup_data.AddDiskFootprint(k42ID, 0, existent_file.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);
  const PUPData::PUP* found_pup = PUPData::GetPUP(k42ID);

  // Add a file, and it should be found.
  base::FilePath temp_file;
  ASSERT_TRUE(CreateTemporaryFileInDir(existent_folder, &temp_file));

  Scan();
  ExpectSinglePUP(k42ID);
  EXPECT_EQ(1UL, found_pup->expanded_disk_footprints.size());
  ExpectDiskFootprint(*found_pup, temp_file);

  ASSERT_TRUE(base::DeleteFile(temp_file, false));
  Scan();
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, ExistentRegistryFootprint) {
  // Set up a registry footprint.
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, nullptr, nullptr,
                                     REGISTRY_VALUE_MATCH_KEY);

  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));

  Scan();
  ExpectSinglePUP(k42ID);

  test_pup_data.Reset({});
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, kValueName, nullptr,
                                     REGISTRY_VALUE_MATCH_VALUE_NAME);
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kComplexValue));

  Scan();
  ExpectSinglePUP(k42ID);

  test_pup_data.Reset({});
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, kValueName, kValue,
                                     REGISTRY_VALUE_MATCH_PARTIAL);

  Scan();
  ExpectSinglePUP(k42ID);
}

TEST_P(ScannerTestWithBitness, RedirectedRegistryFootprint) {
  // This test doesn't use registry overriding because it relies on registry
  // redirection between 32 bit and 64 bit views, which is impossible with
  // registry overriding.

  // Skip the test about 64-bit registry on 32-bit platforms.
  if (GetParam() == 64 &&
      base::win::OSInfo::X86_ARCHITECTURE ==
          base::win::OSInfo::GetInstance()->architecture()) {
    return;
  }

  // Use a GUID for the subkey to reduce the risk of collisions with real
  // software.
  base::string16 subkey =
      base::StrCat({L"software\\cct-", kGUID1Str, L"\\dummy"});

  // Remove existing keys.
  REGSAM registry_views[] = {KEY_WOW64_32KEY, KEY_WOW64_64KEY};
  for (REGSAM view : registry_views) {
    base::win::RegKey test_key;
    if (ERROR_SUCCESS ==
        test_key.Open(HKEY_LOCAL_MACHINE, subkey.c_str(), KEY_READ | view)) {
      test_key.DeleteKey(L"");
    }
  }

  // Add a registry footprint matching under software.
  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     subkey.c_str(), nullptr, nullptr,
                                     REGISTRY_VALUE_MATCH_KEY);

  // Create the key, so that we can look for an existent value.
  REGSAM registry_view = GetParam() == 32 ? KEY_WOW64_32KEY : KEY_WOW64_64KEY;
  ScopedTempRegistryKey system_registry_key(HKEY_LOCAL_MACHINE, subkey.c_str(),
                                            KEY_WRITE | registry_view);
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.Get()->WriteValue(kValueName, kValue));

  // If the current system is 64-bit, check that the created key is not visible
  // under the other registry view, otherwise we test nothing here.
  if (base::win::OSInfo::X64_ARCHITECTURE ==
      base::win::OSInfo::GetInstance()->architecture()) {
    REGSAM other_view = GetParam() == 32 ? KEY_WOW64_64KEY : KEY_WOW64_32KEY;
    base::win::RegKey missing_registry_key;
    EXPECT_EQ(ERROR_FILE_NOT_FOUND,
              missing_registry_key.Open(HKEY_LOCAL_MACHINE, subkey.c_str(),
                                        KEY_READ | other_view));
  }

  Scan();
  ExpectSinglePUP(k42ID);

  const PUPData::PUP* found_pup = PUPData::GetPUP(k42ID);
  EXPECT_EQ(1UL, found_pup->expanded_registry_footprints.size());
  const RegKeyPath key_path(HKEY_LOCAL_MACHINE, subkey.c_str(), registry_view);
  ExpectRegistryFootprint(*found_pup, key_path, L"", L"",
                          REGISTRY_VALUE_MATCH_KEY);
}

TEST_F(ScannerTest, ExistentRegistryFootprintWithExactShortenPath) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath long_name_path(
      scoped_temp_dir.GetPath().Append(kLongFileName));

  // The file must exist for the short to long path name conversion to work.
  base::File file(long_name_path, base::File::FLAG_OPEN_ALWAYS);
  file.Close();

  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(
      k42ID, REGISTRY_ROOT_USERS, kBaseName, kValueName,
      long_name_path.value().c_str(),
      REGISTRY_VALUE_MATCH_COMMON_SEPARATED_SET_CONTAINS_PATH);

  DWORD short_name_len =
      GetShortPathName(long_name_path.value().c_str(), nullptr, 0);
  ASSERT_GT(short_name_len, 0UL);

  base::string16 short_name_path;
  short_name_len = ::GetShortPathName(
      long_name_path.value().c_str(),
      base::WriteInto(&short_name_path, short_name_len), short_name_len);
  ASSERT_GT(short_name_len, 0UL);

  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);

  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_CURRENT_USER,
                                                      kBaseName, KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(
                               kValueName, short_name_path.c_str()));

  Scan();
  ExpectSinglePUP(k42ID);
}

TEST_F(ScannerTest, ExistentRegistryFootprintWithShortenPathSubString) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath long_name_path(
      scoped_temp_dir.GetPath().Append(kLongFileName));

  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(
      k42ID, REGISTRY_ROOT_USERS, kBaseName, kValueName,
      long_name_path.value().c_str(),
      REGISTRY_VALUE_MATCH_COMMON_SEPARATED_SET_CONTAINS_PATH);

  // The file must exist for the short to long path name conversion to work.
  base::File file(long_name_path, base::File::FLAG_OPEN_ALWAYS);
  file.Close();

  DWORD short_name_len =
      GetShortPathName(long_name_path.value().c_str(), nullptr, 0);
  ASSERT_GT(short_name_len, 0UL);

  base::string16 short_name_path;
  short_name_len = ::GetShortPathName(
      long_name_path.value().c_str(),
      base::WriteInto(&short_name_path, short_name_len), short_name_len);
  ASSERT_GT(short_name_len, 0UL);

  DCHECK_GT(PUPData::kCommonDelimitersLength, 2UL);
  base::string16 complete_substr(kDummyFullPath);
  complete_substr += PUPData::kCommonDelimiters[0];
  complete_substr += short_name_path.c_str();
  complete_substr += PUPData::kCommonDelimiters[1];
  complete_substr += kOtherFullPath;

  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);

  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_CURRENT_USER,
                                                      kBaseName, KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(
                               kValueName, complete_substr.c_str()));

  Scan();
  ExpectSinglePUP(k42ID);

  // Now try when the requested substring is at the beginning.
  complete_substr = short_name_path.data();
  complete_substr += PUPData::kCommonDelimiters[2];
  complete_substr += kOtherFullPath;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(
                               kValueName, complete_substr.c_str()));

  Scan();
  ExpectSinglePUP(k42ID);

  // Now try when the requested substring is at the end.
  complete_substr = kOtherFullPath;
  complete_substr += PUPData::kCommonDelimiters[0];
  complete_substr += short_name_path.data();
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(
                               kValueName, complete_substr.c_str()));

  Scan();
  ExpectSinglePUP(k42ID);
}

TEST_F(ScannerTest, ExistentUsersRegistryFootprint) {
  // Set up a users registry footprint.
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);
  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k24ID, REGISTRY_ROOT_USERS, kBaseName,
                                     nullptr, nullptr,
                                     REGISTRY_VALUE_MATCH_KEY);

  base::win::RegKey users_registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            users_registry_key.Create(HKEY_CURRENT_USER, kBaseName, KEY_WRITE));

  Scan();
  ExpectSinglePUP(k24ID);

  test_pup_data.Reset({});
  test_pup_data.AddRegistryFootprint(k24ID, REGISTRY_ROOT_USERS, kBaseName,
                                     kValueName, nullptr,
                                     REGISTRY_VALUE_MATCH_VALUE_NAME);
  ASSERT_EQ(ERROR_SUCCESS, users_registry_key.WriteValue(kValueName, kValue));

  Scan();
  ExpectSinglePUP(k24ID);

  test_pup_data.Reset({});
  test_pup_data.AddRegistryFootprint(k24ID, REGISTRY_ROOT_USERS, kBaseName,
                                     kValueName, kValueDifferentCase,
                                     REGISTRY_VALUE_MATCH_CONTAINS);

  Scan();
  ExpectSinglePUP(k24ID);
}

TEST_F(ScannerTest, NonexistentWildcardRegistryFootprint) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, kWildcardSearch2, nullptr,
                                     REGISTRY_VALUE_MATCH_VALUE_NAME);

  // Make sure the key doesn't exist.
  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_FILE_NOT_FOUND,
            system_registry_key.Open(HKEY_LOCAL_MACHINE, kBaseName, KEY_READ));

  // Now create the key, so that we can look for a nonexistent value.
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kBadValue));

  Scan();
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, ExistentWildcardRegistryFootprint) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Add a registry footprint matching any value names (i.e. "*").
  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, L"*", nullptr,
                                     REGISTRY_VALUE_MATCH_VALUE_NAME);

  // Create the key, so that we can look for an existent value.
  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(kValueName, kValue));

  Scan();
  ExpectSinglePUP(k42ID);
}

TEST_F(ScannerTest, ExistentSingleCharacterWildcardRegistryFootprint) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Add a registry footprint matching any the "bar" name (i.e. "b?r").
  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, L"b?r", nullptr,
                                     REGISTRY_VALUE_MATCH_VALUE_NAME);

  // Create the key, so that we can look for an existent value.
  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(L"bar", kValue));

  Scan();
  ExpectSinglePUP(k42ID);
}

TEST_F(ScannerTest, ExistentMultiCharactersWildcardRegistryFootprint) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Add a registry footprint matching any the "bar" name (i.e. "b*").
  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, L"b*", nullptr,
                                     REGISTRY_VALUE_MATCH_VALUE_NAME);

  // Create the key, so that we can look for an existent value.
  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(L"bar", kValue));

  Scan();
  ExpectSinglePUP(k42ID);
}

TEST_F(ScannerTest, ExistentMixedWildcardRegistryFootprint) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Add a registry footprint matching a temporary filename (i.e. "tmp*.???")
  // which should match the given filename "tmp123456.log".
  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, LR"(tmp*.???)", nullptr,
                                     REGISTRY_VALUE_MATCH_VALUE_NAME);

  // Create the key, so that we can look for an existent value.
  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(L"tmp123456.log", kValue));

  Scan();
  ExpectSinglePUP(k42ID);
}

TEST_F(ScannerTest, KeyPathWithWildCardRegistryFootprint) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Add a registry footprint matching under software.
  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     L"software\\t?st\\d*y", kValueName,
                                     nullptr, REGISTRY_VALUE_MATCH_VALUE_NAME);

  // Create the key, so that we can look for an existent value.
  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                       L"software\\test\\dummy", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(kValueName, kValue));

  Scan();
  ExpectSinglePUP(k42ID);
}

TEST_F(ScannerTest, KeyPathWithNotMatchingWildCardRegistryFootprint) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Add a registry footprint matching under software.
  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     L"software\\t?st\\foo*", kValueName,
                                     nullptr, REGISTRY_VALUE_MATCH_VALUE_NAME);

  // Create the key and a value that doesn't match the registry footprint
  // pattern.
  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                       L"software\\test\\dummy", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(kValueName, kValue));

  Scan();
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, KeyPathWithEscapedWildCardRegistryFootprint) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  // Add a registry footprint matching under software with an escaped wild-card.
  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(
      k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
      L"software\\t?st\\d" ESCAPE_REGISTRY_STR("*") L"y", kValueName, nullptr,
      REGISTRY_VALUE_MATCH_VALUE_NAME);

  // Create the key, so that we can look for an existent value.
  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                       L"software\\test\\dummy", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(kValueName, kValue));

  Scan();
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, MixedFootprints) {
  // Set up disk footprints.
  base::ScopedTempDir scoped_temp_dir1;
  ASSERT_TRUE(scoped_temp_dir1.CreateUniqueTempDir());
  base::FilePath existent_folder(scoped_temp_dir1.GetPath());
  ASSERT_TRUE(base::PathExists(existent_folder));

  base::FilePath temp_file1;
  ASSERT_TRUE(CreateTemporaryFileInDir(existent_folder, &temp_file1));

  TestPUPData test_pup_data;
  test_pup_data.AddDiskFootprint(k42ID, 0, existent_folder.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);

  std::set<UwSId> pups_to_be_scanned;
  pups_to_be_scanned.insert(k42ID);
  std::set<UwSId> pups_to_be_found;
  pups_to_be_found.insert(k42ID);

  base::FilePath nonexistent_file(scoped_temp_dir1.GetPath().Append(kBaseName));
  test_pup_data.AddDiskFootprint(kNonExistentID, 0,
                                 nonexistent_file.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);
  pups_to_be_scanned.insert(kNonExistentID);

  // Set up relative disk footprints.
  base::char16 special_folder_path[kMaxPath];
  HRESULT hr = SHGetFolderPath(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
                               SHGFP_TYPE_CURRENT, special_folder_path);
  ASSERT_EQ(S_OK, hr);

  base::ScopedTempDir scoped_temp_dir2;
  ASSERT_TRUE(scoped_temp_dir2.CreateUniqueTempDirUnderPath(
      base::FilePath(special_folder_path)));

  base::FilePath relative_path(scoped_temp_dir2.GetPath().BaseName());
  test_pup_data.AddDiskFootprint(k24ID, CSIDL_LOCAL_APPDATA,
                                 relative_path.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);
  pups_to_be_scanned.insert(k24ID);
  pups_to_be_found.insert(k24ID);

  base::string16 nonexistent_relative_path(
      base::FilePath(relative_path).Append(kBaseName).value());
  test_pup_data.AddDiskFootprint(kNonExistentID, 0,
                                 nonexistent_relative_path.c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);
  pups_to_be_scanned.insert(kNonExistentID);

  test_pup_data.AddDiskFootprint(k42ID, CSIDL_LOCAL_APPDATA,
                                 relative_path.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);
  pups_to_be_scanned.insert(k42ID);
  pups_to_be_found.insert(k42ID);

  base::FilePath temp_file2;
  ASSERT_TRUE(
      CreateTemporaryFileInDir(scoped_temp_dir2.GetPath(), &temp_file2));

  Scan();
  ExpectFoundPUPs(pups_to_be_found);

  // Set up registry footprints.
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  struct TestData {
    UwSId id;
    const base::char16* key_path;
    const base::char16* value_name;
    const base::char16* value;
    RegistryMatchRule rule;
  } test_data[] = {
      {k24ID, kBaseName, nullptr, nullptr, REGISTRY_VALUE_MATCH_KEY},
      {kNonExistentID, kBadBaseName, kValueName, kValue,
       REGISTRY_VALUE_MATCH_PARTIAL},
      {k12ID, kBaseName, kValueName, kValue, REGISTRY_VALUE_MATCH_PARTIAL},
      {kNonExistentID, kBaseName, kValueName, kBaValue,
       REGISTRY_VALUE_MATCH_PARTIAL},
      {kNonExistentID, kBaseName, kBaValue, kValue,
       REGISTRY_VALUE_MATCH_PARTIAL},
      {k42ID, kBaseName, kValueName, nullptr, REGISTRY_VALUE_MATCH_VALUE_NAME},
  };
  for (size_t i = 0; i < base::size(test_data); ++i) {
    test_pup_data.AddRegistryFootprint(
        test_data[i].id, REGISTRY_ROOT_LOCAL_MACHINE, test_data[i].key_path,
        test_data[i].value_name, test_data[i].value, test_data[i].rule);
    pups_to_be_scanned.insert(test_data[i].id);
  }
  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));
  pups_to_be_found.insert(k24ID);
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kComplexValue));
  pups_to_be_found.insert(k12ID);
  pups_to_be_found.insert(k42ID);

  Scan();
  ExpectFoundPUPs(pups_to_be_found);

  // Set up users registry footprints.
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);

  test_pup_data.AddRegistryFootprint(k12ID, REGISTRY_ROOT_USERS, kBaseName,
                                     kValueName, nullptr,
                                     REGISTRY_VALUE_MATCH_VALUE_NAME);
  pups_to_be_scanned.insert(k12ID);
  test_pup_data.AddRegistryFootprint(kNonExistentID, REGISTRY_ROOT_USERS,
                                     kBadBaseName, kValueName, kValue,
                                     REGISTRY_VALUE_MATCH_EXACT);
  pups_to_be_scanned.insert(kNonExistentID);
  test_pup_data.AddRegistryFootprint(k84ID, REGISTRY_ROOT_USERS, kBaseName,
                                     nullptr, nullptr,
                                     REGISTRY_VALUE_MATCH_KEY);
  pups_to_be_scanned.insert(k84ID);

  base::win::RegKey users_registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            users_registry_key.Create(HKEY_CURRENT_USER, kBaseName, KEY_WRITE));
  pups_to_be_found.insert(k84ID);
  ASSERT_EQ(ERROR_SUCCESS, users_registry_key.WriteValue(kValueName, kValue));
  pups_to_be_found.insert(k12ID);

  Scan();
  ExpectFoundPUPs(pups_to_be_found);
}

TEST_F(ScannerTest, FindSomeWithinMany) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);

  base::win::RegKey users_registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            users_registry_key.Create(HKEY_CURRENT_USER, kBaseName, KEY_WRITE));

  TestPUPData test_pup_data;
  std::set<UwSId> pups_to_find;
  for (UwSId pup_id = 0; pup_id < kManyPUPs; ++pup_id) {
    if (base::RandGenerator(kManyPUPs) % kFoundPUPsModuloBase == 0) {
      test_pup_data.AddRegistryFootprint(pup_id, REGISTRY_ROOT_USERS, kBaseName,
                                         nullptr, nullptr,
                                         REGISTRY_VALUE_MATCH_KEY);
      pups_to_find.insert(pup_id);
    } else {
      test_pup_data.AddRegistryFootprint(pup_id, REGISTRY_ROOT_USERS,
                                         kBadBaseName, nullptr, nullptr,
                                         REGISTRY_VALUE_MATCH_KEY);
    }
  }

  Scan();
  ExpectFoundPUPs(pups_to_find);
}

TEST_F(ScannerTest, DiskMatchActiveFileRule) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath existent_folder(scoped_temp_dir.GetPath());
  ASSERT_TRUE(base::PathExists(existent_folder));

  TestPUPData test_pup_data;
  test_pup_data.AddDiskFootprint(k42ID, 0, existent_folder.value().c_str(),
                                 PUPData::DISK_MATCH_BINARY_FILE);
  const PUPData::PUP* pup42 = PUPData::GetPUP(k42ID);

  // No PUP should be found when no files are found.
  Scan();
  ExpectNoPUPsFound();

  // Add a non-active file, and it should still not be found.
  base::FilePath temp_file1 = existent_folder.Append(L"file.inactive");
  ASSERT_TRUE(CreateEmptyFile(temp_file1));

  // No PUP should be found when no active files are found.
  Scan();
  ExpectNoPUPsFound();

  // Add a active file, and it should be found.
  base::FilePath temp_file2;
  ASSERT_TRUE(CreateTemporaryFileInDir(existent_folder, &temp_file2));

  const base::char16* extensions[] = {
      L".bla", L".dLl", L".ExE", L".foo", L".mshxml", L".URL", L".sys",
  };

  bool found_active = false;
  bool found_not_active = false;
  for (size_t i = 0; i < base::size(extensions); ++i) {
    base::FilePath previous_file(temp_file2);
    temp_file2 = temp_file2.ReplaceExtension(extensions[i]);
    ASSERT_TRUE(ReplaceFile(previous_file, temp_file2, nullptr));
    Scan();
    if (PathHasActiveExtension(temp_file2)) {
      found_active = true;
      ExpectSinglePUP(k42ID);
      EXPECT_EQ(3UL, pup42->expanded_disk_footprints.size());
      ExpectDiskFootprint(*pup42, existent_folder);
      ExpectDiskFootprint(*pup42, temp_file1);
      ExpectDiskFootprint(*pup42, temp_file2);
    } else {
      found_not_active = true;
      ExpectNoPUPsFound();
    }
  }
  EXPECT_TRUE(found_not_active);
  EXPECT_TRUE(found_active);
}

// Creates |num_folders| in |root_folder| returning the deepest folder created.
// All of the created folders are added to |folders_created|, which is
// intentionally not cleared by this function.
// If |num_folders| <= 0, |root_folder| is returned.
// If the directories aren't created, an empty FilePath is returned.
base::FilePath CreateNestedFolders(
    const base::FilePath& root_folder,
    const base::string16& folder_name,
    int num_folders,
    std::vector<base::FilePath>* folders_created) {
  base::FilePath folder(root_folder);
  for (int i = 0; i < num_folders; ++i) {
    folder = folder.Append(base::FilePath(folder_name));
    folders_created->push_back(folder);
  }

  if (!CreateDirectory(folder))
    folder.clear();

  return folder;
}

int DiskMatchFileInFolderDepth(PUPData::DiskMatchRule rule) {
  return rule - PUPData::DISK_MATCH_FILE_IN_FOLDER_DEPTH_1 + 1;
}

TEST_F(ScannerTest, DiskMatchFileInFolderRule) {
  for (auto rule = PUPData::DISK_MATCH_FILE_IN_FOLDER_DEPTH_1;
       rule < PUPData::DISK_MATCH_FILE_IN_FOLDER_END;
       rule = static_cast<PUPData::DiskMatchRule>(rule + 1)) {
    base::ScopedTempDir scoped_temp_dir;
    ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
    base::FilePath root_folder(scoped_temp_dir.GetPath());

    std::vector<base::FilePath> folders;
    base::FilePath sub_folder = CreateNestedFolders(
        root_folder, kBaseName, DiskMatchFileInFolderDepth(rule), &folders);
    ASSERT_FALSE(sub_folder.empty());
    base::FilePath file_to_match(sub_folder.Append(kOtherBaseName));

    TestPUPData test_pup_data;
    test_pup_data.AddDiskFootprint(k42ID, 0, file_to_match.value().c_str(),
                                   rule);
    const PUPData::PUP* pup42 = PUPData::GetPUP(k42ID);

    // No PUP should be found when no files are found.
    Scan();
    ExpectNoPUPsFound();

    // Add the file, and it should be found, as well as all its siblings.
    ASSERT_TRUE(CreateEmptyFile(file_to_match)) << "Rule: " << rule;
    base::FilePath temp_file1;
    ASSERT_TRUE(CreateTemporaryFileInDir(sub_folder, &temp_file1))
        << "Rule: " << rule;
    base::FilePath temp_file2;
    ASSERT_TRUE(CreateTemporaryFileInDir(sub_folder, &temp_file2))
        << "Rule: " << rule;

    Scan();
    ExpectSinglePUP(k42ID);
    EXPECT_EQ(3UL + folders.size(), pup42->expanded_disk_footprints.size())
        << "Rule: " << rule;
    for (const auto& folder : folders) {
      ExpectDiskFootprint(*pup42, folder);
    }
    ExpectDiskFootprint(*pup42, file_to_match);
    ExpectDiskFootprint(*pup42, temp_file1);
    ExpectDiskFootprint(*pup42, temp_file2);
  }
}

TEST_F(ScannerTest, DiskMatchAllFilesInFolderRule) {
  for (auto rule = PUPData::DISK_MATCH_FILE_IN_FOLDER_DEPTH_1;
       rule < PUPData::DISK_MATCH_FILE_IN_FOLDER_END;
       rule = static_cast<PUPData::DiskMatchRule>(rule + 1)) {
    base::ScopedTempDir scoped_temp_dir;
    ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
    base::FilePath root_folder(scoped_temp_dir.GetPath());

    std::vector<base::FilePath> folders;
    base::FilePath sub_folder = CreateNestedFolders(
        root_folder, kBaseName, DiskMatchFileInFolderDepth(rule), &folders);
    ASSERT_FALSE(sub_folder.empty());
    base::FilePath file_to_match(sub_folder.Append(kOtherBaseName));

    TestPUPData test_pup_data;
    test_pup_data.AddDiskFootprint(k42ID, 0, file_to_match.value().c_str(),
                                   rule);
    const PUPData::PUP* pup42 = PUPData::GetPUP(k42ID);

    // No PUP should be found when no files are found.
    Scan();
    ExpectNoPUPsFound();

    // Add the file, and it should be found, as well as a file in each folder.
    ASSERT_TRUE(CreateEmptyFile(file_to_match)) << "Rule: " << rule;
    std::vector<base::FilePath> extra_files;
    for (const auto& folder : folders) {
      base::FilePath extra_file;
      ASSERT_TRUE(CreateTemporaryFileInDir(folder, &extra_file))
          << "Rule: " << rule;
      extra_files.push_back(extra_file);
    }

    Scan();
    ExpectSinglePUP(k42ID);
    // The initial 1 below is for the initial file that is matched.
    EXPECT_EQ(1UL + folders.size() + extra_files.size(),
              pup42->expanded_disk_footprints.size())
        << "Rule: " << rule;
    ExpectDiskFootprint(*pup42, file_to_match);
    for (const auto& folder : folders) {
      ExpectDiskFootprint(*pup42, folder);
    }
    for (const auto& extra_file : extra_files) {
      ExpectDiskFootprint(*pup42, extra_file);
    }
  }
}

TEST_F(ScannerTest, DiskMatchFileIn3FoldersRule) {
  for (auto rule = PUPData::DISK_MATCH_FILE_IN_FOLDER_DEPTH_1;
       rule < PUPData::DISK_MATCH_FILE_IN_FOLDER_END;
       rule = static_cast<PUPData::DiskMatchRule>(rule + 1)) {
    base::ScopedTempDir scoped_temp_dir;
    ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
    base::FilePath root_folder(scoped_temp_dir.GetPath());
    ASSERT_TRUE(base::PathExists(root_folder));

    std::vector<base::FilePath> folders;

    folders.push_back(root_folder.Append(L"abc1"));
    base::FilePath sub_folder =
        CreateNestedFolders(*folders.rbegin(), kBaseName,
                            DiskMatchFileInFolderDepth(rule) - 1, &folders);
    ASSERT_FALSE(sub_folder.empty());
    base::FilePath file_in_folder1(sub_folder.Append(kOtherBaseName));

    folders.push_back(root_folder.Append(L"abc2"));
    sub_folder =
        CreateNestedFolders(*folders.rbegin(), kBaseName,
                            DiskMatchFileInFolderDepth(rule) - 1, &folders);
    ASSERT_FALSE(sub_folder.empty());
    base::FilePath file_in_folder2(sub_folder.Append(kOtherBaseName));

    folders.push_back(root_folder.Append(L"abc3"));
    sub_folder =
        CreateNestedFolders(*folders.rbegin(), kBaseName,
                            DiskMatchFileInFolderDepth(rule) - 1, &folders);
    ASSERT_FALSE(sub_folder.empty());
    base::FilePath file_in_folder3(sub_folder.Append(kOtherBaseName));

    base::FilePath pattern(root_folder.Append(L"abc?"));
    for (int i = 0; i < DiskMatchFileInFolderDepth(rule) - 1; ++i) {
      pattern = pattern.Append(kBaseName);
    }
    pattern = pattern.Append(kOtherBaseName);

    TestPUPData test_pup_data;
    test_pup_data.AddDiskFootprint(k42ID, 0, pattern.value().c_str(), rule);
    const PUPData::PUP* pup42 = PUPData::GetPUP(k42ID);

    // No PUP should be found when no files are found.
    Scan();
    EXPECT_TRUE(found_pups_.empty()) << "Rule: " << rule;

    // Add the files, and they should be found.
    ASSERT_TRUE(CreateEmptyFile(file_in_folder1)) << "Rule: " << rule;
    ASSERT_TRUE(CreateEmptyFile(file_in_folder2)) << "Rule: " << rule;
    ASSERT_TRUE(CreateEmptyFile(file_in_folder3)) << "Rule: " << rule;

    Scan();
    ExpectSinglePUP(k42ID);
    EXPECT_EQ(3UL + folders.size(), pup42->expanded_disk_footprints.size())
        << "Rule: " << rule;
    for (const auto& folder : folders) {
      ExpectDiskFootprint(*pup42, folder);
    }
    ExpectDiskFootprint(*pup42, file_in_folder1);
    ExpectDiskFootprint(*pup42, file_in_folder2);
    ExpectDiskFootprint(*pup42, file_in_folder3);
  }
}

TEST_F(ScannerTest, DiskDontMatchFolderInFolderRule) {
  for (auto rule = PUPData::DISK_MATCH_FILE_IN_FOLDER_DEPTH_1;
       rule < PUPData::DISK_MATCH_FILE_IN_FOLDER_END;
       rule = static_cast<PUPData::DiskMatchRule>(1 + rule)) {
    base::ScopedTempDir scoped_temp_dir;
    ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
    base::FilePath root_folder(scoped_temp_dir.GetPath());

    std::vector<base::FilePath> folders;
    base::FilePath sub_folder = CreateNestedFolders(
        root_folder, kBaseName, DiskMatchFileInFolderDepth(rule), &folders);
    ASSERT_FALSE(sub_folder.empty());
    base::FilePath file_to_match(sub_folder.Append(kBaseName));

    TestPUPData test_pup_data;
    test_pup_data.AddDiskFootprint(k42ID, 0, file_to_match.value().c_str(),
                                   rule);

    // No PUP should be found when no files are found.
    Scan();
    ExpectNoPUPsFound();

    // Add the footprint as a folder, and it should not be found.
    ASSERT_TRUE(CreateDirectory(file_to_match));

    // No PUP should be found when no files are found.
    Scan();
    EXPECT_TRUE(found_pups_.empty()) << "Rule: " << rule;
  }
}

TEST_F(ScannerTest, DiskWithWow64Entry) {
  base::ScopedPathOverride windows_override(base::DIR_WINDOWS);
  base::FilePath windows_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_WINDOWS, &windows_dir));
  base::FilePath native_dir(windows_dir.Append(L"sysnative"));
  ASSERT_TRUE(CreateDirectory(native_dir));

  // Set up a folder with a file in 64-bit system folder.
  ASSERT_TRUE(CreateFileInFolder(native_dir, L"dummy"));

  TestPUPData test_pup_data;
  test_pup_data.AddPUP(k42ID, PUPData::FLAGS_ACTION_REMOVE, nullptr,
                       PUPData::kMaxFilesToRemoveSmallUwS);
  test_pup_data.AddDiskFootprint(k42ID, CSIDL_SYSTEM, L"d?mm?",
                                 PUPData::DISK_MATCH_ANY_FILE);

  Scan();
  ExpectSinglePUP(k42ID);

  const PUPData::PUP* pup = PUPData::GetPUP(k42ID);
  ExpectDiskFootprint(*pup, native_dir.Append(L"dummy"));
}

TEST_F(ScannerTest, RegistryMatchRuleExactValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));

  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, kValueName, kComplexValue,
                                     REGISTRY_VALUE_MATCH_EXACT);

  // Empty key must not match.
  Scan();
  ExpectNoPUPsFound();

  // Write a different value.
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(kValueName, kValue));
  Scan();
  ExpectNoPUPsFound();

  // Write the same value.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kComplexValue));
  Scan();
  ExpectSinglePUP(k42ID);
}

TEST_F(ScannerTest, RegistryMatchRuleContainsValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));

  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, kValueName, kValue,
                                     REGISTRY_VALUE_MATCH_CONTAINS);

  // Empty key must not match.
  Scan();
  ExpectNoPUPsFound();

  // Write a different value.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kValueDifferentCase));
  Scan();
  ExpectSinglePUP(k42ID);

  // Write a different value.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kBaValue));
  Scan();
  ExpectNoPUPsFound();

  // Write the same value.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kComplexValue));
  Scan();
  ExpectSinglePUP(k42ID);
}

TEST_F(ScannerTest, RegistryMatchRulePartialValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));

  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, kValueName, kValue,
                                     REGISTRY_VALUE_MATCH_PARTIAL);

  // Empty key must not match.
  Scan();
  ExpectNoPUPsFound();

  // Write a same value with different case.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kValueDifferentCase));
  Scan();
  ExpectSinglePUP(k42ID);

  // Write a different value.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kBaValue));
  Scan();
  ExpectNoPUPsFound();

  // Write the complex value which contains the value.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kComplexValue));
  Scan();
  ExpectSinglePUP(k42ID);
}

TEST_F(ScannerTest, RegistryMatchRuleCommonSetSeparatedExactValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));

  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(
      k42ID, REGISTRY_ROOT_LOCAL_MACHINE, kBaseName, kValueName, kValue,
      REGISTRY_VALUE_MATCH_COMMON_SEPARATED_SET_EXACT);

  // Write a set separated by common delimiters with the value as an element.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kCommonSetValue));
  Scan();
  ExpectSinglePUP(k42ID);

  // Write a set separated by a null character.
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(
                               kValueName, kCommonSetWithNullValue,
                               sizeof(kCommonSetWithNullValue), REG_SZ));
  Scan();
  ExpectSinglePUP(k42ID);

  // Write a set which contains the value, but isn't equals to.
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(
                               kValueName, kBiggerCommaSetValue,
                               sizeof(kBiggerCommaSetValue), REG_SZ));
  Scan();
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, RegistryMatchRuleCommonSetSeparatedContainsValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));

  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(
      k42ID, REGISTRY_ROOT_LOCAL_MACHINE, kBaseName, kValueName, kValue,
      REGISTRY_VALUE_MATCH_COMMON_SEPARATED_SET_CONTAINS);

  // Write a set separated by common delimiters with the value as an element.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kCommonSetValue));
  Scan();
  ExpectSinglePUP(k42ID);

  // Write a set separated by a null character.
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(
                               kValueName, kCommonSetWithNullValue,
                               sizeof(kCommonSetWithNullValue), REG_SZ));
  Scan();
  ExpectSinglePUP(k42ID);

  // Write a set which contains the value, but isn't equals to.
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(
                               kValueName, kBiggerCommaSetValue,
                               sizeof(kBiggerCommaSetValue), REG_SZ));
  Scan();
  ExpectSinglePUP(k42ID);
}

TEST_F(ScannerTest, RegistryMatchRuleCommaSetSeparatedExactValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));

  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(
      k42ID, REGISTRY_ROOT_LOCAL_MACHINE, kBaseName, kValueName, kValue,
      REGISTRY_VALUE_MATCH_COMMA_SEPARATED_SET_EXACT);

  // Empty key must not match.
  Scan();
  ExpectNoPUPsFound();

  // Write the same value.
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(kValueName, kValue));
  Scan();
  ExpectSinglePUP(k42ID);

  // Write set which contains the exact value.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kCommaSetValue));
  Scan();
  ExpectSinglePUP(k42ID);

  // Write set which contains a partial match.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kBiggerCommaSetValue));
  Scan();
  ExpectNoPUPsFound();

  // Write set without the value.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kBadCommaSetValue));
  Scan();
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, RegistryMatchRuleCommaSetSeparatedContainstValue) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));

  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(
      k42ID, REGISTRY_ROOT_LOCAL_MACHINE, kBaseName, kValueName, kValue,
      REGISTRY_VALUE_MATCH_COMMA_SEPARATED_SET_CONTAINS);

  // Empty key must not match.
  Scan();
  ExpectNoPUPsFound();

  // Write the same value.
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.WriteValue(kValueName, kValue));
  Scan();
  ExpectSinglePUP(k42ID);

  // Write set which contains the exact value.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kCommaSetValue));
  Scan();
  ExpectSinglePUP(k42ID);

  // Write set which contains a partial match.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kBiggerCommaSetValue));
  Scan();
  ExpectSinglePUP(k42ID);

  // Write set without the value.
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kBadCommaSetValue));
  Scan();
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, ReportOnlyPUP) {
  // Set up a folder only disk footprint.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath existent_folder(scoped_temp_dir.GetPath());
  ASSERT_TRUE(base::PathExists(existent_folder));

  TestPUPData test_pup_data;
  test_pup_data.AddPUP(k42ID, PUPData::FLAGS_NONE, nullptr,
                       PUPData::kMaxFilesToRemoveSmallUwS);
  test_pup_data.AddDiskFootprint(k42ID, 0, existent_folder.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);

  // Add a file, and it should be reported but not found.
  base::FilePath temp_file;
  ASSERT_TRUE(CreateTemporaryFileInDir(existent_folder, &temp_file));

  Scan();
  EXPECT_FALSE(found_pups_.empty());
  EXPECT_FALSE(found_pup_to_remove_);
  EXPECT_TRUE(found_report_only_);
}

TEST_F(ScannerTest, StopScanning) {
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_CURRENT_USER);

  base::win::RegKey users_registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            users_registry_key.Create(HKEY_CURRENT_USER, kBaseName, KEY_WRITE));

  TestPUPData test_pup_data;
  std::set<UwSId> pups_to_find;
  for (UwSId pup_id = 0; pup_id < kSomePUPs; ++pup_id) {
    if (base::RandGenerator(kSomePUPs) % kFoundPUPsModuloBase == 0) {
      test_pup_data.AddRegistryFootprint(pup_id, REGISTRY_ROOT_USERS, kBaseName,
                                         nullptr, nullptr,
                                         REGISTRY_VALUE_MATCH_KEY);
      pups_to_find.insert(pup_id);
    } else {
      for (size_t i = 0; i < 100; ++i) {
        test_pup_data.AddRegistryFootprint(pup_id, REGISTRY_ROOT_USERS,
                                           kBadBaseName, nullptr, nullptr,
                                           REGISTRY_VALUE_MATCH_KEY);
        test_pup_data.AddDiskFootprint(pup_id, 0, kBadBaseName,
                                       PUPData::DISK_MATCH_ANY_FILE);
      }
    }
  }

  // Post the stop call to the main loop so that we can start the scan and get
  // it to stop while we wait for it to be done.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ScannerTest::StopScanner, base::Unretained(this)));
  Scan();
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, RunningCustomMatchers) {
  TestPUPData test_pup_data;
  test_pup_data.AddCustomMatcher(k42ID, &TestCustomMatcher1);
  test_pup_data.AddCustomMatcher(k42ID, &TestCustomMatcher3);

  g_test_custom_matcher1_called = false;
  g_test_custom_matcher2_called = false;
  g_test_custom_matcher3_called = false;
  Scan();
  ASSERT_TRUE(g_test_custom_matcher1_called);
  ASSERT_FALSE(g_test_custom_matcher2_called);
  ASSERT_TRUE(g_test_custom_matcher3_called);
  ExpectSinglePUP(k42ID);
}

TEST_F(ScannerTest, RunningCustomMatchersNoScanAndRemove) {
  TestPUPData test_pup_data;
  test_pup_data.AddCustomMatcher(k42ID, &TestCustomMatcher2);

  g_test_custom_matcher1_called = false;
  g_test_custom_matcher2_called = false;
  g_test_custom_matcher3_called = false;
  Scan();
  ASSERT_FALSE(g_test_custom_matcher1_called);
  ASSERT_TRUE(g_test_custom_matcher2_called);
  ASSERT_FALSE(g_test_custom_matcher3_called);
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, RunningCustomMatchersNothingFound) {
  TestPUPData test_pup_data;
  test_pup_data.AddCustomMatcher(k42ID, &TestCustomMatcher3);

  g_test_custom_matcher1_called = false;
  g_test_custom_matcher2_called = false;
  g_test_custom_matcher3_called = false;
  Scan();
  ASSERT_FALSE(g_test_custom_matcher1_called);
  ASSERT_FALSE(g_test_custom_matcher2_called);
  ASSERT_TRUE(g_test_custom_matcher3_called);
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, RunningCustomMatchersWithError) {
  TestPUPData test_pup_data;
  test_pup_data.AddCustomMatcher(k42ID, &TestCustomMatcher4);

  // The custom scanner returns an error, and scanning this pup is aborted.
  Scan();
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, RunningCustomMatchersWithErrorAndFootprint) {
  TestPUPData test_pup_data;
  test_pup_data.AddCustomMatcher(k42ID, &TestCustomMatcher1);
  test_pup_data.AddCustomMatcher(k42ID, &TestCustomMatcher4);

  // The custom scanner returns an error, and scanning this pup is aborted
  // even if there is a footprint matched by |TestCustomMatcher1|.
  Scan();
  ExpectNoPUPsFound();
}

TEST_F(ScannerTest, RunningCustomMatchersKeepDiskActiveFootprintFound) {
  TestPUPData test_pup_data;
  test_pup_data.AddCustomMatcher(k42ID, &TestCustomMatcher5);

  // Add a found disk footprint
  base::ScopedTempDir scoped_temp_dir1;
  ASSERT_TRUE(scoped_temp_dir1.CreateUniqueTempDir());
  base::FilePath existent_folder(scoped_temp_dir1.GetPath());
  ASSERT_TRUE(base::PathExists(existent_folder));
  base::FilePath temp_file;
  ASSERT_TRUE(CreateTemporaryFileInDir(existent_folder, &temp_file));

  test_pup_data.AddDiskFootprint(k42ID, 0, existent_folder.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);

  g_test_custom_matcher5_found = false;
  Scan();
  EXPECT_TRUE(g_test_custom_matcher5_found);
}

TEST_F(ScannerTest, RunningCustomMatchersDefaultActiveFootprintFound) {
  TestPUPData test_pup_data;
  test_pup_data.AddCustomMatcher(k42ID, &TestCustomMatcher5);

  g_test_custom_matcher5_found = false;
  Scan();
  EXPECT_FALSE(g_test_custom_matcher5_found);
}

TEST_F(ScannerTest, ScanRegistrySingleCharacterWildcardFootprints) {
  TestPUPData test_pup_data;
  std::vector<UwSId> pup_ids;
  pup_ids.push_back(k42ID);

  // Set up a local machine registry footprint.
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kRegistryKeyPath, kValueNameSingleWildcard,
                                     nullptr, REGISTRY_VALUE_MATCH_VALUE_NAME);

  base::win::RegKey registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            registry_key.Create(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                                KEY_ALL_ACCESS));

  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(kValueName, kComplexValue));
  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(kValueName1, kComplexValue));
  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(kValueName2, kComplexValue));
  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(kValueName3, kComplexValue));

  Scan();
  ExpectSinglePUP(k42ID);

  const PUPData::PUP* found_pup = PUPData::GetPUP(k42ID);
  EXPECT_EQ(3UL, found_pup->expanded_registry_footprints.size());

  // |kValueName| should not match the regular expression but |kValueName1|,
  // |kValueName2| and |kValueName3| must be matched.
  const RegKeyPath key_path(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_WOW64_32KEY);
  ExpectRegistryFootprint(*found_pup, key_path, kValueName1, L"",
                          REGISTRY_VALUE_MATCH_VALUE_NAME);

  ExpectRegistryFootprint(*found_pup, key_path, kValueName2, L"",
                          REGISTRY_VALUE_MATCH_VALUE_NAME);

  ExpectRegistryFootprint(*found_pup, key_path, kValueName3, L"",
                          REGISTRY_VALUE_MATCH_VALUE_NAME);
}

TEST_F(ScannerTest, ScanWithRegistryMultiCharactersWildcardFootprints) {
  TestPUPData test_pup_data;
  std::vector<UwSId> pup_ids;
  pup_ids.push_back(k42ID);

  // Set up a local machine registry footprint.
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kRegistryKeyPath, kValueNameMultiWildcard,
                                     nullptr, REGISTRY_VALUE_MATCH_VALUE_NAME);

  base::win::RegKey registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            registry_key.Create(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                                KEY_ALL_ACCESS));

  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(kOtherBaseName, kFooValue));
  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(kBaseName, kComplexValue));
  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(kValueName1, kComplexValue));
  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(kValueName2, kComplexValue));
  ASSERT_EQ(ERROR_SUCCESS, registry_key.WriteValue(kValueName3, kComplexValue));

  Scan();
  ExpectSinglePUP(k42ID);

  // The footprint |kOtherBaseName| is not matched by the regular expression.
  // |kBaseName|, |kValueName1|, |kValueName2| and |kValueName3| must be
  // matched.
  const PUPData::PUP* found_pup = PUPData::GetPUP(k42ID);
  EXPECT_EQ(4UL, found_pup->expanded_registry_footprints.size());

  const RegKeyPath key_path(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_WOW64_32KEY);
  ExpectRegistryFootprint(*found_pup, key_path, kBaseName, L"",
                          REGISTRY_VALUE_MATCH_VALUE_NAME);

  ExpectRegistryFootprint(*found_pup, key_path, kValueName1, L"",
                          REGISTRY_VALUE_MATCH_VALUE_NAME);

  ExpectRegistryFootprint(*found_pup, key_path, kValueName2, L"",
                          REGISTRY_VALUE_MATCH_VALUE_NAME);

  ExpectRegistryFootprint(*found_pup, key_path, kValueName3, L"",
                          REGISTRY_VALUE_MATCH_VALUE_NAME);
}

TEST_F(ScannerTest, ScanWithRegistryWildcardAndRuleFootprints) {
  TestPUPData test_pup_data;
  std::vector<UwSId> pup_ids;
  pup_ids.push_back(k42ID);

  // Set up a local machine registry footprint.
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  test_pup_data.AddRegistryFootprint(
      k42ID, REGISTRY_ROOT_LOCAL_MACHINE, kRegistryKeyPath,
      kValueNameSingleWildcard, kFooValue,
      REGISTRY_VALUE_MATCH_COMMON_SEPARATED_SET_EXACT);

  base::win::RegKey registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            registry_key.Create(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                                KEY_ALL_ACCESS));

  ASSERT_EQ(ERROR_SUCCESS,
            registry_key.WriteValue(kValueName1, kSetWithCommonValue1));
  ASSERT_EQ(ERROR_SUCCESS,
            registry_key.WriteValue(kValueName2, kSetWithCommonValue2));
  ASSERT_EQ(ERROR_SUCCESS,
            registry_key.WriteValue(kValueName3, kSetWithCommonValue3));

  Scan();
  ExpectSinglePUP(k42ID);

  const PUPData::PUP* found_pup = PUPData::GetPUP(k42ID);
  EXPECT_EQ(3UL, found_pup->expanded_registry_footprints.size());

  const RegKeyPath key_path(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_WOW64_32KEY);
  ExpectRegistryFootprint(*found_pup, key_path, kValueName1, kFooValue,
                          REGISTRY_VALUE_MATCH_COMMON_SEPARATED_SET_EXACT);

  ExpectRegistryFootprint(*found_pup, key_path, kValueName2, kFooValue,
                          REGISTRY_VALUE_MATCH_COMMON_SEPARATED_SET_EXACT);

  ExpectRegistryFootprint(*found_pup, key_path, kValueName3, kFooValue,
                          REGISTRY_VALUE_MATCH_COMMON_SEPARATED_SET_EXACT);
}

TEST_F(ScannerTest, ScanWithKeyPathWildCardsRegistryFootprints) {
  TestPUPData test_pup_data;
  std::vector<UwSId> pup_ids;
  pup_ids.push_back(k42ID);

  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     L"software\\t*\\du??y", L"b*", nullptr,
                                     REGISTRY_VALUE_MATCH_VALUE_NAME);

  base::win::RegKey test1_registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            test1_registry_key.Create(
                HKEY_LOCAL_MACHINE, L"software\\test1\\dummy", KEY_ALL_ACCESS));
  base::win::RegKey test2_registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            test2_registry_key.Create(
                HKEY_LOCAL_MACHINE, L"software\\test2\\dummy", KEY_ALL_ACCESS));

  base::win::RegKey best_registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            best_registry_key.Create(HKEY_LOCAL_MACHINE,
                                     L"software\\best\\dummy", KEY_ALL_ACCESS));

  base::win::RegKey unused_registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            unused_registry_key.Create(HKEY_LOCAL_MACHINE, L"software\\best",
                                       KEY_ALL_ACCESS));

  ASSERT_EQ(ERROR_SUCCESS, test1_registry_key.WriteValue(L"foo", kValue));
  ASSERT_EQ(ERROR_SUCCESS, test1_registry_key.WriteValue(L"bar", kValue));
  ASSERT_EQ(ERROR_SUCCESS, test1_registry_key.WriteValue(L"bat", kValue));
  ASSERT_EQ(ERROR_SUCCESS, test2_registry_key.WriteValue(L"foo", kValue));
  ASSERT_EQ(ERROR_SUCCESS, test2_registry_key.WriteValue(L"bar", kValue));
  ASSERT_EQ(ERROR_SUCCESS, test2_registry_key.WriteValue(L"bat", kValue));
  ASSERT_EQ(ERROR_SUCCESS, best_registry_key.WriteValue(L"bat", kValue));

  Scan();
  ExpectSinglePUP(k42ID);

  const PUPData::PUP* found_pup = PUPData::GetPUP(k42ID);
  EXPECT_EQ(4UL, found_pup->expanded_registry_footprints.size());

  const RegKeyPath key_path(HKEY_LOCAL_MACHINE, L"software\\test1\\dummy",
                            KEY_WOW64_32KEY);
  ExpectRegistryFootprint(*found_pup, key_path, L"bar", L"",
                          REGISTRY_VALUE_MATCH_VALUE_NAME);

  ExpectRegistryFootprint(*found_pup, key_path, L"bat", L"",
                          REGISTRY_VALUE_MATCH_VALUE_NAME);

  ExpectRegistryFootprint(*found_pup, key_path, L"bar", L"",
                          REGISTRY_VALUE_MATCH_VALUE_NAME);

  ExpectRegistryFootprint(*found_pup, key_path, L"bat", L"",
                          REGISTRY_VALUE_MATCH_VALUE_NAME);
}

TEST_F(ScannerTest, ScanRegistryKeyWithEscapedStar) {
  TestPUPData test_pup_data;
  std::vector<UwSId> pup_ids;
  pup_ids.push_back(k42ID);

  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  constexpr base::char16 kRegistryPathWithStar[] = L"foo\\*";
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     L"foo\\" ESCAPE_REGISTRY_STR("*"), nullptr,
                                     nullptr, REGISTRY_VALUE_MATCH_KEY);

  base::win::RegKey test_registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            test_registry_key.Create(HKEY_LOCAL_MACHINE, kRegistryPathWithStar,
                                     KEY_ALL_ACCESS));

  ASSERT_EQ(ERROR_SUCCESS,
            test_registry_key.Create(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                                     KEY_ALL_ACCESS));
  Scan();
  ExpectSinglePUP(k42ID);

  // As the regular expression contains an escaped wild-card, only the exact
  // key |kRegistryPathWithStar| must be matched. |kRegistryKeyPath| must not
  // be matched.
  const PUPData::PUP* found_pup = PUPData::GetPUP(k42ID);
  EXPECT_EQ(1UL, found_pup->expanded_registry_footprints.size());

  ExpectRegistryFootprint(
      *found_pup, RegKeyPath(HKEY_LOCAL_MACHINE, L"foo\\*", KEY_WOW64_32KEY),
      L"", L"", REGISTRY_VALUE_MATCH_KEY);
}

TEST_F(ScannerTest, RegistryFootprintForDetectOnlyUws) {
  TestPUPData test_pup_data;
  test_pup_data.AddPUP(k12ID, PUPData::FLAGS_NONE, "test",
                       PUPData::kMaxFilesToRemoveSmallUwS);
  test_pup_data.AddPUP(k24ID, PUPData::FLAGS_NONE, "test2",
                       PUPData::kMaxFilesToRemoveSmallUwS);

  // Set up a local machine registry footprint.
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  test_pup_data.AddRegistryFootprint(k12ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kRegistryKeyPath, nullptr, nullptr,
                                     REGISTRY_VALUE_MATCH_KEY);

  test_pup_data.AddRegistryFootprint(k24ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kRegistryKeyPath, nullptr, nullptr,
                                     REGISTRY_VALUE_MATCH_KEY);

  base::win::RegKey registry_key;
  ASSERT_EQ(ERROR_SUCCESS,
            registry_key.Create(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                                KEY_ALL_ACCESS));

  Scan();

  const RegKeyPath key_path(HKEY_LOCAL_MACHINE, kRegistryKeyPath,
                            KEY_WOW64_32KEY);

  std::set<UwSId> pups_to_be_found;
  pups_to_be_found.insert(k12ID);
  pups_to_be_found.insert(k24ID);
  ExpectFoundPUPs(pups_to_be_found);

  const PUPData::PUP* found_pup12 = PUPData::GetPUP(k12ID);
  ExpectRegistryFootprint(*found_pup12, key_path, L"", L"",
                          REGISTRY_VALUE_MATCH_KEY);

  const PUPData::PUP* found_pup24 = PUPData::GetPUP(k24ID);
  ExpectRegistryFootprint(*found_pup24, key_path, L"", L"",
                          REGISTRY_VALUE_MATCH_KEY);
}

TEST_F(ScannerTest, FindOnlyOneDiskFootprint) {
  base::ScopedTempDir scoped_temp_dir1;
  ASSERT_TRUE(scoped_temp_dir1.CreateUniqueTempDir());
  base::FilePath existent_folder_1(scoped_temp_dir1.GetPath());
  ASSERT_TRUE(base::PathExists(existent_folder_1));
  base::FilePath temp_file_1;
  ASSERT_TRUE(CreateTemporaryFileInDir(existent_folder_1, &temp_file_1));
  base::FilePath temp_file_2;
  ASSERT_TRUE(CreateTemporaryFileInDir(existent_folder_1, &temp_file_2));

  base::ScopedTempDir scoped_temp_dir2;
  ASSERT_TRUE(scoped_temp_dir2.CreateUniqueTempDir());
  base::FilePath existent_folder_2(scoped_temp_dir2.GetPath());
  ASSERT_TRUE(base::PathExists(existent_folder_2));
  base::FilePath temp_file_3;
  ASSERT_TRUE(CreateTemporaryFileInDir(existent_folder_2, &temp_file_3));

  TestPUPData test_pup_data;
  test_pup_data.AddDiskFootprint(k42ID, 0, temp_file_1.value().c_str(),
                                 PUPData::DISK_MATCH_FILE_IN_FOLDER_DEPTH_1);
  test_pup_data.AddDiskFootprint(k42ID, 0, temp_file_3.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);
  const PUPData::PUP* found_pup = PUPData::GetPUP(k42ID);

  options_.set_only_one_footprint(false);
  Scan();
  ExpectSinglePUP(k42ID);
  EXPECT_EQ(4UL, found_pup->expanded_disk_footprints.size());
  ExpectDiskFootprint(*found_pup, temp_file_1);
  ExpectDiskFootprint(*found_pup, temp_file_2);
  ExpectDiskFootprint(*found_pup, existent_folder_1);
  ExpectDiskFootprint(*found_pup, temp_file_3);

  options_.set_only_one_footprint(true);
  Scan();
  ExpectSinglePUP(k42ID);
  ASSERT_EQ(1UL, found_pups_.size());
  EXPECT_EQ(1UL, found_pup->expanded_disk_footprints.size());
  ExpectDiskFootprint(*found_pup, temp_file_1);
}

TEST_F(ScannerTest, FindOnlyOneRegistryFootprints) {
  // Set up a registry footprint.
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  TestPUPData test_pup_data;
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, nullptr, nullptr,
                                     REGISTRY_VALUE_MATCH_KEY);
  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));
  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, kValueName, nullptr,
                                     REGISTRY_VALUE_MATCH_VALUE_NAME);
  ASSERT_EQ(ERROR_SUCCESS,
            system_registry_key.WriteValue(kValueName, kComplexValue));

  const PUPData::PUP* found_pup = PUPData::GetPUP(k42ID);

  options_.set_only_one_footprint(false);
  Scan();
  ExpectSinglePUP(k42ID);
  EXPECT_EQ(2UL, found_pup->expanded_registry_footprints.size());

  options_.set_only_one_footprint(true);
  Scan();
  ExpectSinglePUP(k42ID);
  EXPECT_EQ(1UL, found_pup->expanded_registry_footprints.size());
}

TEST_F(ScannerTest, FindOnlyOneCustomMatchers) {
  TestPUPData test_pup_data;
  test_pup_data.AddCustomMatcher(k42ID, &TestCustomMatcher1);
  test_pup_data.AddCustomMatcher(k42ID, &TestCustomMatcher2);

  const PUPData::PUP* found_pup = PUPData::GetPUP(k42ID);

  options_.set_only_one_footprint(false);
  Scan();
  ExpectSinglePUP(k42ID);
  EXPECT_EQ(2UL, found_pup->expanded_disk_footprints.size());
  ExpectDiskFootprint(*found_pup, base::FilePath(kDummyFullPath));
  ExpectDiskFootprint(*found_pup, base::FilePath(kOtherFullPath));
  EXPECT_TRUE(g_test_custom_matcher1_called);
  EXPECT_TRUE(g_test_custom_matcher2_called);

  options_.set_only_one_footprint(true);
  Scan();
  ExpectSinglePUP(k42ID);
  EXPECT_EQ(1UL, found_pup->expanded_disk_footprints.size());
  ExpectDiskFootprint(*found_pup, base::FilePath(kDummyFullPath));
  EXPECT_TRUE(g_test_custom_matcher1_called);
  EXPECT_FALSE(g_test_custom_matcher2_called);
}

TEST_F(ScannerTest, FindOnlyOneAmongAllTypes) {
  // Start with just a custom matcher.
  TestPUPData test_pup_data;
  test_pup_data.AddCustomMatcher(k42ID, &TestCustomMatcher1);

  const PUPData::PUP* found_pup = PUPData::GetPUP(k42ID);

  options_.set_only_one_footprint(true);
  Scan();
  ExpectSinglePUP(k42ID);
  EXPECT_EQ(1UL, found_pup->expanded_disk_footprints.size());
  EXPECT_EQ(0UL, found_pup->expanded_registry_footprints.size());
  ExpectDiskFootprint(*found_pup, base::FilePath(kDummyFullPath));
  EXPECT_TRUE(g_test_custom_matcher1_called);

  // Now add a registry footprint and only it should be found.
  registry_util::RegistryOverrideManager registry_override;
  registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);

  test_pup_data.AddRegistryFootprint(k42ID, REGISTRY_ROOT_LOCAL_MACHINE,
                                     kBaseName, nullptr, nullptr,
                                     REGISTRY_VALUE_MATCH_KEY);
  base::win::RegKey system_registry_key;
  ASSERT_EQ(ERROR_SUCCESS, system_registry_key.Create(HKEY_LOCAL_MACHINE,
                                                      kBaseName, KEY_WRITE));

  Scan();
  ExpectSinglePUP(k42ID);
  EXPECT_EQ(0UL, found_pup->expanded_disk_footprints.size());
  EXPECT_EQ(1UL, found_pup->expanded_registry_footprints.size());
  EXPECT_FALSE(g_test_custom_matcher1_called);

  // Now add a disk footprint and onlyit should be found.
  // Set up a folder only disk footprint.
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  base::FilePath existent_folder(scoped_temp_dir.GetPath());
  ASSERT_TRUE(base::PathExists(existent_folder));
  base::FilePath temp_file;
  ASSERT_TRUE(CreateTemporaryFileInDir(existent_folder, &temp_file));

  test_pup_data.AddDiskFootprint(k42ID, 0, temp_file.value().c_str(),
                                 PUPData::DISK_MATCH_ANY_FILE);

  Scan();
  ExpectSinglePUP(k42ID);
  EXPECT_EQ(1UL, found_pup->expanded_disk_footprints.size());
  EXPECT_EQ(0UL, found_pup->expanded_registry_footprints.size());
  ExpectDiskFootprint(*found_pup, temp_file);
  EXPECT_FALSE(g_test_custom_matcher1_called);
}

}  // namespace chrome_cleaner
