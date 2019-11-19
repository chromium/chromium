// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/disk_util.h"

#include <windows.h>

#include <shlobj.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_shortcut_win.h"
#include "base/test/test_timeouts.h"
#include "base/unguessable_token.h"
#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_wrapper.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/strings/string_util.h"
#include "chrome/chrome_cleaner/test/test_executables.h"
#include "chrome/chrome_cleaner/test/test_file_util.h"
#include "chrome/chrome_cleaner/test/test_layered_service_provider.h"
#include "chrome/chrome_cleaner/test/test_strings.h"
#include "chrome/chrome_cleaner/test/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

// A path name under program files.
const wchar_t kProgramFilesBaseName[] = L"Foo";

// Keep these digests in sorted order.
const char* const kFileContentDigests[] = {
    "02544E052F29BBA79C81243EC63B43B6CD85B185461928E65BFF501346C62A75",
    "04614470DDF4939091F5EC4A13C92A9EAAACF07CA5C3F713E792E2D21CD24075",
    // Hash for content: |kFileContent2|.
    "82E0B92772BC0DA59AAB0B9231AA006FB37B4F99EC3E853C5A62786A1C7215BD",
    "94F7BDF53CDFDE7AA5E5C90BCDA6793B7377CE39E2591ABC758EBAE8072A275C",
    // Hash for content: |kFileContent1|.
    "BD283E41A3672B6BDAA574F8BD7176F8BCA95BD81383CDE32AA6D78B1DB0E371"};

const wchar_t kFileName1[] = L"Filename one";
const wchar_t kFileName2[] = L"Filename two";
const wchar_t kFileName3[] = L"Third filename";
const wchar_t kLongFileName1[] = L"Long File Name.bla";
const wchar_t kLongFileName2[] = L"Other Long File Name.bla";
const char kFileContent1[] = "This is the file content.";
const char kFileContent2[] = "Hi!";
const char kFileContent3[] = "Hello World!";

const internal::FileInformation kFileInformation1(L"some/path/something.tmp",
                                                  "3/1/2016",
                                                  "3/3/2016",
                                                  "somedigest1234",
                                                  9876,
                                                  L"Company Name",
                                                  L"CNShort",
                                                  L"Product Name",
                                                  L"PNShort",
                                                  L"Internal Name",
                                                  L"Something_Original.tmp",
                                                  L"Very descriptive",
                                                  L"42.1.2");

const wchar_t kFileInformation1ExpectedString[] =
    L"path = 'some/path/something.tmp', file_creation_date = "
    L"'3/1/2016', file_last_modified_date = '3/3/2016', digest = "
    L"'somedigest1234', size = '9876', company_name = 'Company Name', "
    L"company_short_name = 'CNShort', product_name = 'Product Name', "
    L"product_short_name = 'PNShort', internal_name = 'Internal Name', "
    L"original_filename = 'Something_Original.tmp', file_description = 'Very "
    L"descriptive', file_version = '42.1.2', active_file = '0'";

// All potential format of content that get from registry. %ls will be replaced
// by the executable path.
// clang-format off
const wchar_t* kMockRegistryContents[] = {
    // Straight path as is.
    L"%ls",
    L"\"%ls\"",
    // With command line arguments.
    L"%ls -s",
    L"\"%ls\" -s",
    L"\"%ls -s\"",
    // Using rundll.
    L"C:\\Windows\\SysWow64\\rundll32 %ls",
    L"C:\\Windows\\SysWow64\\rundll32 \"%ls\"",
    // Using rundll with args.
    L"C:\\Windows\\SysWow64\\rundll32.exe %ls,OpenAs_RunDLL %%1",
    L"C:\\Windows\\SysWow64\\rundll32.exe \"%ls\",OpenAs_RunDLL %%1",
    L"C:\\Windows\\SysWow64\\rundll32.exe \"%ls,OpenAs_RunDLL\" %%1",
    L"C:\\Windows\\SysWow64\\rundll32.exe %ls a1 a2,OpenAs_RunDLL %%1",
    L"C:\\Windows\\SysWow64\\rundll32.exe \"%ls\" a1,OpenAs_RunDLL %%1",
    L"C:\\Windows\\SysWow64\\rundll32.exe \"%ls a1,OpenAs_RunDLL\" %%1",
    L"C:\\Windows\\System32\\rundll32 %ls",
    // Rundll without a path.
    L"rundll32.exe %ls a1 a2,OpenAs_RunDLL %%1",
    L"rundll32.exe \"%ls\" -s %%1",
    L"rundll32.exe \"%ls -s\" %%1",
    // Rundll without extension.
    L"C:\\Windows\\SysWow64\\rundll32 %ls a1 a2,OpenAs_RunDLL %%1",
    L"C:\\Windows\\SysWow64\\rundll32 \"%ls\" -s %%1",
    L"C:\\Windows\\SysWow64\\rundll32 \"%ls -s\" %%1"};
// clang-format on

bool LaunchTestProcess(const wchar_t* executable,
                       const char* action,
                       bool wait) {
  base::FilePath executable_path(executable);
  base::CommandLine command_line(executable_path);
  command_line.AppendSwitch(action);
  base::LaunchOptions options = base::LaunchOptions();
  options.wait = wait;
  return base::LaunchProcess(command_line, options).IsValid();
}

bool DoesVolumeSupportNamedStreams(const base::FilePath& path) {
  std::vector<base::string16> components;
  path.GetComponents(&components);
  DCHECK(!components.empty());
  base::string16& drive = components[0];
  drive += L'\\';
  DWORD system_flags = 0;
  if (::GetVolumeInformation(drive.c_str(), nullptr, 0, nullptr, nullptr,
                             &system_flags, nullptr, 0) != TRUE) {
    PLOG(ERROR) << "Cannot retrieve drive information: '" << path.value()
                << "'.";
    return false;
  }

  return (system_flags & FILE_NAMED_STREAMS) != 0;
}

void CreateProgramPathsAndFiles(const base::FilePath& temp_dir_path,
                                base::FilePath* program_path,
                                base::FilePath* spaced_program_path) {
  DCHECK(program_path);
  DCHECK(spaced_program_path);
  base::FilePath folder;
  base::FilePath file_path;
  base::FilePath spaced_folder;
  base::FilePath spaced_file_path;

  ASSERT_TRUE(base::CreateTemporaryDirInDir(temp_dir_path, L"folder", &folder));
  ASSERT_TRUE(base::CreateTemporaryDirInDir(temp_dir_path, L"  spaced folder",
                                            &spaced_folder));

  ASSERT_TRUE(CreateTemporaryFileInDir(folder, &file_path));
  ASSERT_TRUE(CreateTemporaryFileInDir(spaced_folder, &spaced_file_path));

  *program_path = file_path;
  *spaced_program_path = spaced_file_path;
}

// Substitutes |registry_path| into each entry in the mock registry in turn
// and passes the result to ExtractExecutablePathFromRegistryContent to get a
// file path. Returns success if every file path matches |expected_path|.
::testing::AssertionResult ExtractExecutablePathFromMockRegistryAndExpect(
    const base::string16& registry_path,
    const base::FilePath& expected_path) {
  for (const base::string16& registry_content : kMockRegistryContents) {
    base::string16 full_registry_content =
        base::StringPrintf(registry_content.c_str(), registry_path.c_str());
    base::FilePath extracted_path =
        ExtractExecutablePathFromRegistryContent(full_registry_content);
    if (!PathEqual(expected_path, extracted_path)) {
      return ::testing::AssertionFailure()
             << expected_path.value() << " != " << extracted_path.value()
             << ", full_registry_content = " << full_registry_content;
    }
  }
  return ::testing::AssertionSuccess();
}

// Return that the sample DLL is whitelisted and all other files are not.
bool WhitelistSampleDLL(const base::FilePath& path) {
  return PathEqual(path, GetSampleDLLPath());
}

}  // namespace

TEST(DiskUtilTests, GetX64ProgramFilePath) {
  base::FilePath x64_program_files =
      GetX64ProgramFilesPath(base::FilePath(kProgramFilesBaseName));
  if (base::win::OSInfo::GetArchitecture() ==
      base::win::OSInfo::X86_ARCHITECTURE) {
    EXPECT_TRUE(x64_program_files.empty());
    return;
  }

  EXPECT_FALSE(x64_program_files.empty());
  EXPECT_NE(x64_program_files,
            ExpandSpecialFolderPath(CSIDL_PROGRAM_FILES,
                                    base::FilePath(kProgramFilesBaseName)));
}

TEST(DiskUtilTests, PathContainsWildcards) {
  EXPECT_FALSE(PathContainsWildcards(base::FilePath(L"c:\\foo.txt")));
  EXPECT_FALSE(PathContainsWildcards(base::FilePath(L"c:\\bar\\foo.txt")));
  EXPECT_TRUE(PathContainsWildcards(base::FilePath(L"c:\\foo.t?t")));
  EXPECT_TRUE(PathContainsWildcards(base::FilePath(L"c:\\foo.t*t")));
  EXPECT_TRUE(
      PathContainsWildcards(base::FilePath(L"c:\\"
                                           LR"(???)"
                                           L"*\\foo.txt")));
  EXPECT_TRUE(PathContainsWildcards(base::FilePath(L"*:\\")));
  EXPECT_FALSE(PathContainsWildcards(base::FilePath(L"foo.txt")));
  EXPECT_TRUE(PathContainsWildcards(base::FilePath(LR"(foo.???)")));
}

TEST(DiskUtilTests, CollectMatchingPathsMultipleWildcards) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create files and folders under |temp_dir| and add them to |expected_files|.
  std::set<base::FilePath> expected_files;

  base::ScopedTempDir sub_dir1;
  ASSERT_TRUE(sub_dir1.CreateUniqueTempDirUnderPath(temp_dir.GetPath()));

  base::ScopedTempDir sub_dir2;
  ASSERT_TRUE(sub_dir2.CreateUniqueTempDirUnderPath(temp_dir.GetPath()));

  base::FilePath sub_dir1_file_path1(sub_dir1.GetPath().Append(kFileName1));
  base::File sub_dir1_file1(sub_dir1_file_path1, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir1_file_path1));

  base::FilePath sub_dir1_file_path2(sub_dir1.GetPath().Append(kFileName2));
  base::File sub_dir1_file2(sub_dir1_file_path2, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir1_file_path2));

  base::FilePath sub_dir1_file_path3(sub_dir1.GetPath().Append(kFileName3));
  base::File sub_dir1_file3(sub_dir1_file_path3, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir1_file_path3));

  base::FilePath sub_dir2_file_path1(sub_dir2.GetPath().Append(kFileName1));
  base::File sub_dir2_file1(sub_dir2_file_path1, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir2_file_path1));

  base::FilePath sub_dir2_file_path2(sub_dir2.GetPath().Append(kFileName2));
  base::File sub_dir2_file2(sub_dir2_file_path2, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir2_file_path2));

  expected_files.insert(sub_dir1_file_path1);
  expected_files.insert(sub_dir1_file_path2);
  expected_files.insert(sub_dir2_file_path1);
  expected_files.insert(sub_dir2_file_path2);

  base::FilePath wildcard_path1(
      temp_dir.GetPath().Append(L"*").Append(L"Filename*"));
  std::vector<base::FilePath> matches;
  CollectMatchingPaths(wildcard_path1, &matches);
  EXPECT_THAT(expected_files, testing::UnorderedElementsAreArray(matches));
}

TEST(DiskUtilTests, CollectMatchingPathsDriveWildcard) {
  std::vector<base::FilePath> matches;

  // The drive could not be enumerated, thus no file will exists..
  base::FilePath wildcard_path1(L"*:\\test");
  CollectMatchingPaths(wildcard_path1, &matches);
  EXPECT_TRUE(matches.empty());

  base::FilePath wildcard_path2(L"*:\\");
  CollectMatchingPaths(wildcard_path2, &matches);
  EXPECT_TRUE(matches.empty());
}

TEST(DiskUtilTests, CollectMatchingPathsDirectoryWildcard) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create files and folders under |temp_dir| and add them to |expected_files|.
  std::set<base::FilePath> expected_files;

  base::ScopedTempDir sub_dir1;
  ASSERT_TRUE(sub_dir1.CreateUniqueTempDirUnderPath(temp_dir.GetPath()));

  base::ScopedTempDir sub_dir2;
  ASSERT_TRUE(sub_dir2.CreateUniqueTempDirUnderPath(temp_dir.GetPath()));

  base::FilePath sub_dir1_file_path1(sub_dir1.GetPath().Append(kFileName1));
  base::File sub_dir1_file1(sub_dir1_file_path1, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir1_file_path1));

  base::FilePath sub_dir1_file_path2(sub_dir1.GetPath().Append(kFileName2));
  base::File sub_dir1_file2(sub_dir1_file_path2, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir1_file_path2));

  base::FilePath sub_dir1_file_path3(sub_dir1.GetPath().Append(kFileName3));
  base::File sub_dir1_file3(sub_dir1_file_path3, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir1_file_path3));

  base::FilePath sub_dir2_file_path1(sub_dir2.GetPath().Append(kFileName1));
  base::File sub_dir2_file1(sub_dir2_file_path1, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir2_file_path1));

  base::FilePath sub_dir2_file_path2(sub_dir2.GetPath().Append(kFileName2));
  base::File sub_dir2_file2(sub_dir2_file_path2, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir2_file_path2));

  expected_files.insert(sub_dir1_file_path1);
  expected_files.insert(sub_dir2_file_path1);
  base::FilePath wildcard_path3(
      temp_dir.GetPath().Append(L"*").Append(kFileName1));
  std::vector<base::FilePath> matches;
  CollectMatchingPaths(wildcard_path3, &matches);
  EXPECT_THAT(expected_files, testing::UnorderedElementsAreArray(matches));
}

TEST(DiskUtilTests, CollectMatchingPathsMultipleFileWildcards) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::ScopedTempDir sub_dir1;
  ASSERT_TRUE(sub_dir1.CreateUniqueTempDirUnderPath(temp_dir.GetPath()));

  base::ScopedTempDir sub_dir2;
  ASSERT_TRUE(sub_dir2.CreateUniqueTempDirUnderPath(temp_dir.GetPath()));

  base::FilePath sub_dir1_file_path1(sub_dir1.GetPath().Append(kFileName1));
  base::File sub_dir1_file1(sub_dir1_file_path1, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir1_file_path1));

  base::FilePath sub_dir1_file_path2(sub_dir1.GetPath().Append(kFileName2));
  base::File sub_dir1_file2(sub_dir1_file_path2, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir1_file_path2));

  base::FilePath sub_dir1_file_path3(sub_dir1.GetPath().Append(kFileName3));
  base::File sub_dir1_file3(sub_dir1_file_path3, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir1_file_path3));

  base::FilePath sub_dir2_file_path1(sub_dir2.GetPath().Append(kFileName1));
  base::File sub_dir2_file1(sub_dir2_file_path1, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir2_file_path1));

  base::FilePath sub_dir2_file_path2(sub_dir2.GetPath().Append(kFileName2));
  base::File sub_dir2_file2(sub_dir2_file_path2, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir2_file_path2));

  // Create files and folders under |temp_dir| and add them to |expected_files|.
  std::set<base::FilePath> expected_files;
  expected_files.insert(sub_dir1_file_path3);
  base::FilePath wildcard_path4(
      temp_dir.GetPath().Append(L"*").Append(L"*Third*"));
  std::vector<base::FilePath> matches;
  CollectMatchingPaths(wildcard_path4, &matches);
  EXPECT_THAT(expected_files, testing::UnorderedElementsAreArray(matches));
}

TEST(DiskUtilTests, CollectMatchingPathsNoWildcards) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::ScopedTempDir sub_dir;
  ASSERT_TRUE(sub_dir.CreateUniqueTempDirUnderPath(temp_dir.GetPath()));

  base::FilePath sub_dir_file_path1(sub_dir.GetPath().Append(kFileName1));
  base::File sub_dir_file1(sub_dir_file_path1, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir_file_path1));

  base::FilePath sub_dir_file_path2(sub_dir.GetPath().Append(kFileName2));
  base::File sub_dir_file2(sub_dir_file_path2, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir_file_path2));

  base::FilePath sub_dir_file_path3(sub_dir.GetPath().Append(kFileName3));
  base::File sub_dir_file3(sub_dir_file_path3, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir_file_path3));

  // Create files and folders under |temp_dir| and add them to |expected_files|.
  std::vector<base::FilePath> matches;

  base::FilePath no_wildcard_path(sub_dir.GetPath());
  CollectMatchingPaths(no_wildcard_path, &matches);
  EXPECT_EQ(1UL, matches.size());
  EXPECT_TRUE(base::Contains(matches, sub_dir.GetPath()));
}

TEST(DiskUtilTests, CollectExecutableMatchingPaths) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath subfolder1_path;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(temp_dir.GetPath(), L"sub1",
                                            &subfolder1_path));

  base::FilePath subfolder2_path;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(temp_dir.GetPath(), L"sub2",
                                            &subfolder2_path));

  base::FilePath subfolder3_path(subfolder2_path.Append(L"folder.exe"));
  ASSERT_TRUE(base::CreateDirectory(subfolder3_path));

  base::FilePath file_path1(subfolder1_path.Append(L"dummy1.exe"));
  base::FilePath file_path2(subfolder1_path.Append(L"dummy2.exe"));
  base::FilePath file_path3(subfolder2_path.Append(L"dummy3.exe"));
  base::FilePath file_path4(subfolder2_path.Append(L"dummy4.exe"));
  base::FilePath file_path5(subfolder2_path.Append(L"info.exe.txt"));
  base::FilePath file_path6(subfolder3_path.Append(L"bad-mad.exe"));

  CreateFileWithContent(file_path1, kFileContent1, sizeof(kFileContent1));
  CreateFileWithContent(file_path2, kFileContent2, sizeof(kFileContent2));
  CreateFileWithContent(file_path3, kFileContent3, sizeof(kFileContent3));
  CreateFileWithContent(file_path4, kFileContent1, sizeof(kFileContent1));
  CreateFileWithContent(file_path5, kFileContent2, sizeof(kFileContent2));
  CreateFileWithContent(file_path6, kFileContent2, sizeof(kFileContent2));

  base::FilePath wildcard_path(temp_dir.GetPath().Append(L"*\\*.exe"));
  std::vector<base::FilePath> executable_matches;
  CollectMatchingPaths(wildcard_path, &executable_matches);

  EXPECT_THAT(executable_matches,
              testing::ElementsAre(file_path1, file_path2, file_path3,
                                   file_path4, subfolder3_path));
}

TEST(DiskUtilTests, CollectMultipleDotWildcardsMatchingPaths) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath subfolder1_path;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(temp_dir.GetPath(), L"sub1",
                                            &subfolder1_path));

  base::FilePath subfolder2_path;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(temp_dir.GetPath(), L"sub2",
                                            &subfolder2_path));

  base::FilePath subfolder3_path;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(subfolder2_path, L"folder.exe",
                                            &subfolder3_path));

  base::FilePath file_path1(subfolder1_path.Append(L"dummy1.tar.gz"));
  base::FilePath file_path2(subfolder1_path.Append(L"dummy2.12.exe"));
  base::FilePath file_path3(subfolder2_path.Append(L"dummy3.tar.gz"));
  base::FilePath file_path4(subfolder2_path.Append(L"dummy4.12.exe"));
  base::FilePath file_path5(subfolder2_path.Append(L"info.tar.gz.txt"));
  base::FilePath file_path6(subfolder3_path.Append(L"bad-mad.exe"));

  CreateFileWithContent(file_path1, kFileContent1, sizeof(kFileContent1));
  CreateFileWithContent(file_path2, kFileContent2, sizeof(kFileContent2));
  CreateFileWithContent(file_path3, kFileContent3, sizeof(kFileContent3));
  CreateFileWithContent(file_path4, kFileContent1, sizeof(kFileContent1));
  CreateFileWithContent(file_path5, kFileContent2, sizeof(kFileContent2));
  CreateFileWithContent(file_path6, kFileContent2, sizeof(kFileContent2));

  base::FilePath wildcard_path_tar_gz(
      temp_dir.GetPath().Append(L"*\\*.tar.gz"));
  std::vector<base::FilePath> executable_matches_tar_gz;
  CollectMatchingPaths(wildcard_path_tar_gz, &executable_matches_tar_gz);

  EXPECT_THAT(executable_matches_tar_gz,
              testing::ElementsAre(file_path1, file_path3));

  base::FilePath wildcard_path_all(temp_dir.GetPath().Append(L"*\\*.*.*"));
  std::vector<base::FilePath> executable_matches_all;
  CollectMatchingPaths(wildcard_path_all, &executable_matches_all);

  EXPECT_THAT(executable_matches_all,
              testing::ElementsAre(file_path1, file_path2, file_path3,
                                   file_path4, file_path5));
}

TEST(DiskUtilTests, CollectEmptyDirMatchingPaths) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath subfolder1_path;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(temp_dir.GetPath(), L"sub1",
                                            &subfolder1_path));

  base::FilePath subfolder2_path;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(temp_dir.GetPath(), L"sub2",
                                            &subfolder2_path));

  base::FilePath wildcard_path1(temp_dir.GetPath().Append(L"*\\dummy.exe"));
  std::vector<base::FilePath> executable_matches;
  CollectMatchingPaths(wildcard_path1, &executable_matches);
  EXPECT_TRUE(executable_matches.empty());
}

TEST(DiskUtilTests, CollectCumulativeMatchingPaths) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::ScopedTempDir sub_dir1;
  ASSERT_TRUE(sub_dir1.CreateUniqueTempDirUnderPath(temp_dir.GetPath()));

  // Create files and folders under |sub_dirX| and add them to |expected_files|.
  std::set<base::FilePath> expected_files;
  base::FilePath sub_dir1_file_path1(sub_dir1.GetPath().Append(kFileName1));
  base::File sub_dir1_file1(sub_dir1_file_path1, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir1_file_path1));
  expected_files.insert(sub_dir1_file_path1);

  base::FilePath sub_dir1_file_path2(sub_dir1.GetPath().Append(kFileName2));
  base::File sub_dir1_file2(sub_dir1_file_path2, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir1_file_path2));
  expected_files.insert(sub_dir1_file_path2);

  base::ScopedTempDir sub_dir2;
  ASSERT_TRUE(sub_dir2.CreateUniqueTempDirUnderPath(temp_dir.GetPath()));

  base::FilePath sub_dir2_file_path1(sub_dir2.GetPath().Append(kFileName1));
  base::File sub_dir2_file1(sub_dir2_file_path1, base::File::FLAG_CREATE);
  ASSERT_TRUE(base::PathExists(sub_dir2_file_path1));
  expected_files.insert(sub_dir2_file_path1);

  base::FilePath wildcard_path1(sub_dir1.GetPath().Append(L"*"));
  std::vector<base::FilePath> matches;
  CollectMatchingPaths(wildcard_path1, &matches);
  base::FilePath wildcard_path2(sub_dir2.GetPath().Append(L"*"));
  CollectMatchingPaths(wildcard_path2, &matches);
  EXPECT_THAT(expected_files, testing::UnorderedElementsAreArray(matches));
}

TEST(DiskUtilTests, PathHasActiveExtension) {
  EXPECT_TRUE(PathHasActiveExtension(base::FilePath(L"C:\\uws\\file.exe")));
  EXPECT_TRUE(PathHasActiveExtension(base::FilePath(L"C:\\uws\\file.exe  ")));
  EXPECT_TRUE(PathHasActiveExtension(base::FilePath(L"C:\\uws\\file.jpg.exe")));
  EXPECT_TRUE(PathHasActiveExtension(base::FilePath(L"C:\\uws\\file.lnk")));
  EXPECT_TRUE(PathHasActiveExtension(base::FilePath(L"C:\\uws\\file")));
  EXPECT_TRUE(PathHasActiveExtension(base::FilePath(L"C:\\file.lnk::$DATA")));

  EXPECT_FALSE(PathHasActiveExtension(base::FilePath(L"C:\\uws\\file.wvm")));
  EXPECT_FALSE(PathHasActiveExtension(base::FilePath(L"C:\\uws\\file.jpg")));
  EXPECT_FALSE(PathHasActiveExtension(base::FilePath(L"C:\\uws\\file.jpg ")));
  EXPECT_FALSE(PathHasActiveExtension(base::FilePath(L"C:\\file.txt::$DATA")));
}

TEST(DiskUtilTests, ExpandEnvPath) {
  ASSERT_TRUE(
      ::SetEnvironmentVariable(L"CLEANER_TEST_VAR", L"CLEANER_TEST_VALUE"));
  ASSERT_TRUE(::SetEnvironmentVariable(L"ROOT_TEST_VAR", L"c:\\root"));
  base::FilePath test_path1(L"C:\\%CLEANER_TEST_VAR%\\test\\foo");
  base::FilePath test_path2(L"%ROOT_TEST_VAR%\\test\\foo");
  base::FilePath test_path3(
      L"C:\\aa%CLEANER_TEST_VAR%bb\\test\\%%CLEANER_TEST_VAR%%");

  base::FilePath expanded_path;
  ExpandEnvPath(test_path1, &expanded_path);
  EXPECT_EQ(L"C:\\CLEANER_TEST_VALUE\\test\\foo", expanded_path.value());

  ExpandEnvPath(test_path2, &expanded_path);
  EXPECT_EQ(L"c:\\root\\test\\foo", expanded_path.value());

  ExpandEnvPath(test_path3, &expanded_path);
  EXPECT_EQ(L"C:\\aaCLEANER_TEST_VALUEbb\\test\\%CLEANER_TEST_VALUE%",
            expanded_path.value());
}

TEST(DiskUtilTests, ExpandWow64Path) {
  base::ScopedPathOverride windows_override(
      CsidlToPathServiceKey(CSIDL_WINDOWS));
  base::FilePath windows_folder;
  ASSERT_TRUE(base::PathService::Get(CsidlToPathServiceKey(CSIDL_WINDOWS),
                                     &windows_folder));

  base::FilePath system_folder(windows_folder.Append(L"system32"));
  base::ScopedPathOverride system_override(CsidlToPathServiceKey(CSIDL_SYSTEM),
                                           system_folder, true, true);

  base::FilePath native_folder(windows_folder.Append(L"sysnative"));
  ASSERT_TRUE(base::CreateDirectory(native_folder));

  base::FilePath file_path1(system_folder.Append(kFileName1));
  base::FilePath file_path2(native_folder.Append(kFileName2));
  base::FilePath file_path3_system(system_folder.Append(kFileName3));
  base::FilePath file_path3_native(native_folder.Append(kFileName3));
  CreateFileWithContent(file_path1, kFileContent1, sizeof(kFileContent1));
  CreateFileWithContent(file_path2, kFileContent2, sizeof(kFileContent2));
  CreateFileWithContent(file_path3_system, kFileContent3,
                        sizeof(kFileContent3));
  CreateFileWithContent(file_path3_native, kFileContent3,
                        sizeof(kFileContent3));

  base::FilePath expanded_file1;
  ExpandWow64Path(file_path1, &expanded_file1);
  ASSERT_TRUE(PathEqual(expanded_file1, file_path1));

  base::FilePath expanded_file2;
  ExpandWow64Path(file_path2, &expanded_file2);
  ASSERT_TRUE(PathEqual(expanded_file2, file_path2));

  base::FilePath expanded_file3;
  ExpandWow64Path(file_path3_system, &expanded_file3);
  ASSERT_TRUE(PathEqual(expanded_file3, file_path3_native));
}

TEST(DiskUtilTests, ComputeSHA256DigestOfPath) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Check the digest of an non-existing file.
  base::FilePath file_path1(temp_dir.GetPath().Append(kFileName1));
  std::string digest1;
  EXPECT_FALSE(ComputeSHA256DigestOfPath(file_path1, &digest1));
  EXPECT_TRUE(digest1.empty());

  // Create an empty file and validate the digest.
  base::FilePath file_path2(temp_dir.GetPath().Append(kFileName2));
  base::File empty_file(file_path2, base::File::FLAG_CREATE);
  empty_file.Close();

  std::string digest2;
  EXPECT_TRUE(ComputeSHA256DigestOfPath(file_path2, &digest2));
  EXPECT_STREQ(
      "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855",
      digest2.c_str());

  // Create a file with some content and validate the digest.
  base::FilePath file_path3(temp_dir.GetPath().Append(kFileName3));
  base::File valid_file(
      file_path3, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_EQ(sizeof(kFileContent),
            static_cast<size_t>(valid_file.WriteAtCurrentPos(
                kFileContent, sizeof(kFileContent))));
  valid_file.Close();

  std::string digest3;
  EXPECT_TRUE(ComputeSHA256DigestOfPath(file_path3, &digest3));
  EXPECT_STREQ(
      "BD283E41A3672B6BDAA574F8BD7176F8BCA95BD81383CDE32AA6D78B1DB0E371",
      digest3.c_str());
}

TEST(DiskUtilTests, ComputeSHA256DigestOfPathOnBigFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  struct DigestInfo {
    size_t size;
    const char* digest;
  } digests[] = {
      {1, "CA978112CA1BBDCAFAC231B39A23DC4DA786EFF8147C4E72B9807785AFEE48BB"},
      {2, "FB8E20FC2E4C3F248C60C39BD652F3C1347298BB977B8B4D5903B85055620603"},
      {3, "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"},
      {100, "2AC123DCD759EEBABFA1B17C0332B88B3815EF3F95FBFCCEB5FAC07E233235BD"},
      {128, "6C05BE2C4268843AE47E68E611277CE62C02153F2F4D2E1E2A1A4B44F766CF74"},
      {1000,
       "915E53A44C18B19BB06BA5B3F5FCAF1DC4651E8404C63425CFC6174E74659D87"},
      {1023,
       "6A6EE128AAC6B98D2697EED0A912AE264603D046B3CBFD5E7EA1D01C865474D9"},
      {1024,
       "DBA4A6315B76548B7A4DD079EF6AA29A7B34FA8B92C11668473441715C5F0AF5"},
      {1025,
       "2B4B65474580781B4DC0AB66B9A0F39B869DE5A44CF26DBA22AC0496760D4230"},
      {4095,
       "7413609B553226A9A8A3203A82062111DC1F98C24163E303774F27E4F615BFB2"},
      {4096,
       "BC45051AC426475F459EC0B0C88A6646D037B8DFB1B9FA3CA3EF9203CE33E283"},
      {4097,
       "A9145EB3812CA8F11A014029FEE1854FABB76D2FFEA680D0875F78FA786F58B8"},
      {10000,
       "5B92F844F0ED521B75688F4B6FF58E127711709613589EB6EC88FDFBBDC7DC63"}};

  for (size_t offset = 0; offset < base::size(digests); ++offset) {
    DigestInfo* info = &digests[offset];
    DCHECK(info);

    // Create a file and write some content into it.
    base::FilePath file_path(temp_dir.GetPath().Append(kFileName1));
    base::File valid_file(
        file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    for (size_t position = 0; position < info->size; ++position) {
      char c = 'a' + (position % 26);
      ASSERT_EQ(1, valid_file.WriteAtCurrentPos(&c, 1));
    }
    valid_file.Close();

    std::string digest;
    EXPECT_TRUE(ComputeSHA256DigestOfPath(file_path, &digest));
    EXPECT_STREQ(info->digest, digest.c_str());
  }
}

TEST(DiskUtilTests, ComputeSHA256DigestOfString) {
  std::string digest_result;
  std::string content(kFileContent2, sizeof(kFileContent2));
  EXPECT_TRUE(ComputeSHA256DigestOfString(content, &digest_result));
  EXPECT_STREQ(kFileContentDigests[2], digest_result.c_str());
}

TEST(DiskUtilTests, GetLayeredServiceProviders) {
  // Make sure that running the OS implementation doesn't crash/dcheck.
  LSPPathToGUIDs providers;
  GetLayeredServiceProviders(LayeredServiceProviderWrapper(), &providers);
  providers.clear();

  // Make sure an empty test provider returns nothing
  TestLayeredServiceProvider lsp;
  GetLayeredServiceProviders(lsp, &providers);
  EXPECT_TRUE(providers.empty());

  // Now try with a couple of providers
  base::FilePath file_path1 = base::FilePath(kFileName1);
  base::FilePath file_path2 = base::FilePath(kFileName2);
  lsp.AddProvider(kGUID1, file_path1);
  lsp.AddProvider(kGUID2, file_path2);
  lsp.AddProvider(kGUID3, file_path2);

  GetLayeredServiceProviders(lsp, &providers);

  EXPECT_EQ(2UL, providers.size());
  EXPECT_NE(providers.end(), providers.find(file_path1));
  EXPECT_NE(providers.end(), providers.find(file_path2));
  EXPECT_EQ(1UL, providers.find(file_path1)->second.size());
  EXPECT_NE(providers.find(file_path1)->second.end(),
            providers.find(file_path1)->second.find(kGUID1));
  EXPECT_EQ(2UL, providers.find(file_path2)->second.size());
  EXPECT_NE(providers.find(file_path2)->second.end(),
            providers.find(file_path2)->second.find(kGUID2));
  EXPECT_NE(providers.find(file_path2)->second.end(),
            providers.find(file_path2)->second.find(kGUID3));
}

TEST(DiskUtilTests, DeleteFileFromTempProcess) {
  base::FilePath test_file;
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::CreateTemporaryFileInDir(test_dir.GetPath(), &test_file);
  ASSERT_TRUE(base::PathExists(test_file));
  base::WriteFile(test_file, "foo", 3);
  base::win::ScopedHandle process_handle;
  EXPECT_TRUE(DeleteFileFromTempProcess(test_file, 0, &process_handle));
  ASSERT_NE(static_cast<HANDLE>(nullptr), process_handle.Get());
  DWORD wait_result = ::WaitForSingleObject(
      process_handle.Get(),
      TestTimeouts::action_max_timeout().InMilliseconds());
  process_handle.Close();
  EXPECT_EQ(WAIT_OBJECT_0, wait_result);
  EXPECT_FALSE(base::PathExists(test_file));
  EXPECT_FALSE(DeleteFileFromTempProcess(test_file, 0, &process_handle));
  EXPECT_FALSE(process_handle.IsValid());
}

TEST(DiskUtilTests, PathEqual) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath long_path1(temp_dir.GetPath().Append(kLongFileName1));
  base::FilePath long_path2(temp_dir.GetPath().Append(kLongFileName2));
  base::FilePath long_path1_upper(base::ToUpperASCII(long_path1.value()));

  base::FilePath short_path1;
  CreateFileAndGetShortName(long_path1, &short_path1);

  // Same paths are equal.
  EXPECT_TRUE(PathEqual(long_path1, long_path1));
  EXPECT_TRUE(PathEqual(long_path2, long_path2));
  // Same paths with different case are equal.
  EXPECT_TRUE(PathEqual(long_path1, long_path1_upper));
  // Different path are not equal.
  EXPECT_FALSE(PathEqual(long_path1, long_path2));

  // Short and long path to the same file are equal.
  EXPECT_TRUE(PathEqual(short_path1, long_path1));
  // Short and long path to different files are not equal.
  EXPECT_FALSE(PathEqual(short_path1, long_path2));
}

TEST(DiskUtilTests, GetAppDataProductDirectory) {
  base::ScopedPathOverride appdata_override(
      CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA));
  base::FilePath appdata_dir;
  ASSERT_TRUE(base::PathService::Get(CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA),
                                     &appdata_dir));

  base::FilePath product_folder;
  EXPECT_TRUE(GetAppDataProductDirectory(&product_folder));
  EXPECT_TRUE(base::DirectoryExists(product_folder));
  EXPECT_TRUE(PathEqual(appdata_dir, product_folder.DirName().DirName()));
}

TEST(DiskUtilTests, ZoneIdentifier) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath path(temp_dir.GetPath().Append(kTestProcessExecutableName));

  if (!DoesVolumeSupportNamedStreams(temp_dir.GetPath())) {
    LOG(ERROR) << "Skip ZoneIdentifier : alternate streams not supported.";
    return;
  }

  // Copy the test_process executable in a temporary folder.
  base::FilePath executable_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_path));
  base::FilePath source_path =
      executable_path.Append(kTestProcessExecutableName);
  ASSERT_TRUE(base::CopyFile(source_path, path));

  // Overwrite the ZoneIdentifier.
  EXPECT_FALSE(HasZoneIdentifier(path));
  EXPECT_TRUE(OverwriteZoneIdentifier(path));
  EXPECT_TRUE(HasZoneIdentifier(path));

  // Validate the content of the Zone.Identifier stream.
  base::FilePath stream_path(path.value() + L":Zone.Identifier");
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(stream_path, &content));
  EXPECT_EQ("[ZoneTransfer]\r\nZoneId=0\r\n", content);
}

TEST(DiskUtilTests, ZoneIdentifierWhenProcessIsRunning) {
  base::FilePath executable_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_path));

  if (!DoesVolumeSupportNamedStreams(executable_path)) {
    LOG(ERROR) << "Skip ZoneIdentifier : alternate streams not supported.";
    return;
  }

  // Copy the test_process executable to a temporary name. We don't use a
  // ScopedTempDir here because in a component build, the executable depends on
  // DLL's that would have to be copied into the folder too.
  base::FilePath source_exe_path(
      executable_path.Append(kTestProcessExecutableName));
  base::string16 target_exe_name = base::StrCat(
      {base::UTF8ToUTF16(base::UnguessableToken::Create().ToString()),
       L".exe"});
  base::FilePath target_exe_path(executable_path.Append(target_exe_name));

  ASSERT_TRUE(base::CopyFile(source_exe_path, target_exe_path));
  base::ScopedClosureRunner delete_temp_file(base::BindOnce(
      [](const base::FilePath& temp_file) {
        base::DeleteFile(temp_file, /*recursive=*/false);
      },
      target_exe_path));

  // Launch the test_process and wait it's completion. The process must set its
  // zone identifier.
  EXPECT_FALSE(HasZoneIdentifier(target_exe_path));
  ASSERT_FALSE(IsProcessRunning(target_exe_name.c_str()));
  ASSERT_TRUE(LaunchTestProcess(target_exe_path.value().c_str(),
                                kTestForceOverwriteZoneIdentifier, false));
  EXPECT_TRUE(WaitForProcessesStopped(target_exe_name.c_str()));
  EXPECT_TRUE(HasZoneIdentifier(target_exe_path));

  // Validate the content of the Zone.Identifier stream.
  base::FilePath stream_path(target_exe_path.value() + L":Zone.Identifier");
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(stream_path, &content));
  EXPECT_EQ("[ZoneTransfer]\r\nZoneId=0\r\n", content);
}

TEST(DiskUtilTests,
     ExtractExecutablePathFromRegistryContentWithSysnativeReplacement) {
  base::FilePath system_folder;
  ASSERT_TRUE(base::PathService::Get(CsidlToPathServiceKey(CSIDL_SYSTEM),
                                     &system_folder));

  base::FilePath native_folder(system_folder.DirName().Append(L"sysnative"));
  // Only run this test on 64-bits Windows with 32-bits process.
  if (base::PathExists(native_folder)) {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDirUnderPath(native_folder));

    base::FilePath program_path, spaced_program_path;
    CreateProgramPathsAndFiles(temp_dir.GetPath(), &program_path,
                               &spaced_program_path);
    const base::FilePath program_paths[] = {program_path, spaced_program_path};
    for (const auto& program_path : program_paths) {
      // convert C:\\Windows\sysnative\scoped_folder\folder1234\file1 into
      // C:\\Windows\system32\scoped_folder\folder1234\file1
      base::FilePath program_path_system =
          system_folder.Append(program_path.DirName().DirName().BaseName())
              .Append(program_path.DirName().BaseName())
              .Append(program_path.BaseName());
      EXPECT_TRUE(ExtractExecutablePathFromMockRegistryAndExpect(
          program_path_system.value(), program_path));
    }
  }
}

TEST(DiskUtilTests, ExtractExecutablePathFromRegistryContentWithEnvVariable) {
  // This test expects files to be placed in %TEMP% and not anywhere else
  // ScopedTempDir might decide to put them.
  base::string16 temp_str;
  ASSERT_NE(0U, ::GetEnvironmentVariableW(
                    L"TEMP", ::base::WriteInto(&temp_str, MAX_PATH), MAX_PATH))
      << logging::SystemErrorCodeToString(logging::GetLastSystemErrorCode());
  base::FilePath temp_path(temp_str);
  ASSERT_TRUE(base::PathExists(temp_path));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDirUnderPath(temp_path));

  base::FilePath program_path, spaced_program_path;
  CreateProgramPathsAndFiles(temp_dir.GetPath(), &program_path,
                             &spaced_program_path);
  const base::FilePath program_paths[] = {program_path, spaced_program_path};

  for (const auto& program_path : program_paths) {
    // Convert
    // "C:\Users\$USER\AppData\Local\Temp\scoped_dir1234\folder6788\A111.tmp"
    // into "scoped_dir1234\folder6789\A111.tmp"
    base::FilePath relative_program_path =
        program_path.DirName()
            .DirName()
            .BaseName()
            .Append(program_path.DirName().BaseName())
            .Append(program_path.BaseName());
    const auto program_path_with_var =
        base::StrCat({L"%TEMP%\\", relative_program_path.value()});

    EXPECT_TRUE(ExtractExecutablePathFromMockRegistryAndExpect(
        program_path_with_var, program_path));
  }
}

TEST(DiskUtilTests, ExtractExecutablePathFromRegistryContent) {
  // Create the executable to be recognized.
  base::ScopedTempDir temp_dir;

  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath program_path, spaced_program_path;
  CreateProgramPathsAndFiles(temp_dir.GetPath(), &program_path,
                             &spaced_program_path);
  const base::FilePath program_paths[] = {program_path, spaced_program_path};

  for (const auto& program_path : program_paths) {
    EXPECT_TRUE(ExtractExecutablePathFromMockRegistryAndExpect(
        program_path.value(), program_path));
  }
}

TEST(DiskUtilTests, FilePathLess) {
  base::FilePath path_a(L"c:\\a");
  base::FilePath path_upper_a(L"c:\\A");
  base::FilePath path_b(L"c:\\b");
  FilePathLess file_path_less;
  EXPECT_TRUE(file_path_less(path_a, path_b));
  EXPECT_TRUE(file_path_less(path_upper_a, path_b));
  EXPECT_FALSE(file_path_less(path_b, path_a));
  EXPECT_FALSE(file_path_less(path_a, path_upper_a));
  EXPECT_FALSE(file_path_less(path_upper_a, path_a));

  std::map<base::FilePath, int, FilePathLess> collection;
  collection[path_a] = 0;
  EXPECT_NE(collection.find(path_a), collection.end());
  EXPECT_NE(collection.find(path_upper_a), collection.end());
  EXPECT_EQ(collection.find(path_b), collection.end());
}

TEST(DiskUtilTests, RetrieveDetailedFileInformation) {
  base::ScopedPathOverride appdata_override(
      CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA));
  base::FilePath appdata_folder;

  ASSERT_TRUE(base::PathService::Get(CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA),
                                     &appdata_folder));

  base::FilePath temp_file(appdata_folder.Append(L"DUMMY.inactive"));
  CreateFileWithContent(temp_file, kFileContent1, sizeof(kFileContent1));

  bool whitelisted = false;
  internal::FileInformation file_information;
  RetrieveDetailedFileInformation(temp_file, &file_information, &whitelisted);

  EXPECT_FALSE(whitelisted);

  base::string16 sanitized_path = SanitizePath(temp_file);

  EXPECT_EQ(sanitized_path, file_information.path);
  EXPECT_FALSE(file_information.creation_date.empty());
  EXPECT_FALSE(file_information.last_modified_date.empty());
  EXPECT_FALSE(file_information.active_file);
  EXPECT_EQ(kFileContentDigests[4], file_information.sha256);
  EXPECT_EQ(sizeof(kFileContent1), static_cast<size_t>(file_information.size));
  // The next fields are parsed from PE headers so they won't exist.
  EXPECT_TRUE(file_information.company_name.empty());
  EXPECT_TRUE(file_information.company_short_name.empty());
  EXPECT_TRUE(file_information.product_name.empty());
  EXPECT_TRUE(file_information.product_short_name.empty());
  EXPECT_TRUE(file_information.internal_name.empty());
  EXPECT_TRUE(file_information.original_filename.empty());
  EXPECT_TRUE(file_information.file_description.empty());
  EXPECT_TRUE(file_information.file_version.empty());
}

TEST(DiskUtilTests, RetrieveDetailedFileInformationNoFile) {
  base::FilePath appdata_folder;

  ASSERT_TRUE(base::PathService::Get(CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA),
                                     &appdata_folder));

  base::FilePath non_existent_file(
      appdata_folder.DirName().Append(L"abcd1234CCT1234.tmp"));

  bool whitelisted = false;
  internal::FileInformation file_information;
  RetrieveDetailedFileInformation(non_existent_file, &file_information,
                                  &whitelisted);

  EXPECT_FALSE(whitelisted);
  EXPECT_TRUE(file_information.path.empty());
  EXPECT_TRUE(file_information.creation_date.empty());
  EXPECT_TRUE(file_information.last_modified_date.empty());
  EXPECT_FALSE(file_information.active_file);
  EXPECT_TRUE(file_information.sha256.empty());
  EXPECT_EQ(0, file_information.size);
  EXPECT_TRUE(file_information.company_name.empty());
  EXPECT_TRUE(file_information.company_short_name.empty());
  EXPECT_TRUE(file_information.product_name.empty());
  EXPECT_TRUE(file_information.product_short_name.empty());
  EXPECT_TRUE(file_information.internal_name.empty());
  EXPECT_TRUE(file_information.original_filename.empty());
  EXPECT_TRUE(file_information.file_description.empty());
  EXPECT_TRUE(file_information.file_version.empty());
}

TEST(DiskUtilTests, RetrieveDetailedFileInformationWhitelisted) {
  bool whitelisted = false;
  internal::FileInformation file_information;

  RetrieveDetailedFileInformation(GetSampleDLLPath(), &file_information,
                                  &whitelisted,
                                  base::BindOnce(&WhitelistSampleDLL));

  EXPECT_TRUE(whitelisted);
  EXPECT_TRUE(file_information.path.empty());
  EXPECT_TRUE(file_information.creation_date.empty());
  EXPECT_TRUE(file_information.last_modified_date.empty());
  EXPECT_FALSE(file_information.active_file);
  EXPECT_TRUE(file_information.sha256.empty());
  EXPECT_EQ(0, file_information.size);
  EXPECT_TRUE(file_information.company_name.empty());
  EXPECT_TRUE(file_information.company_short_name.empty());
  EXPECT_TRUE(file_information.product_name.empty());
  EXPECT_TRUE(file_information.product_short_name.empty());
  EXPECT_TRUE(file_information.internal_name.empty());
  EXPECT_TRUE(file_information.original_filename.empty());
  EXPECT_TRUE(file_information.file_description.empty());
  EXPECT_TRUE(file_information.file_version.empty());
}

TEST(DiskUtilTests, RetrieveBasicFileInformation) {
  base::ScopedPathOverride appdata_override(
      CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA));
  base::FilePath appdata_folder;

  ASSERT_TRUE(base::PathService::Get(CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA),
                                     &appdata_folder));

  base::FilePath temp_file(appdata_folder.Append(L"DUMMY.exe"));
  CreateFileWithContent(temp_file, kFileContent1, sizeof(kFileContent1));

  internal::FileInformation file_information;
  RetrieveBasicFileInformation(temp_file, &file_information);

  // The expected file path value should be sanitized.
  EXPECT_EQ(SanitizePath(temp_file), file_information.path);
  EXPECT_FALSE(file_information.creation_date.empty());
  EXPECT_FALSE(file_information.last_modified_date.empty());
  EXPECT_EQ(sizeof(kFileContent1), static_cast<size_t>(file_information.size));
  EXPECT_TRUE(file_information.active_file);
  // The next fields are not included in basic file information.
  EXPECT_TRUE(file_information.sha256.empty());
  EXPECT_TRUE(file_information.company_name.empty());
  EXPECT_TRUE(file_information.company_short_name.empty());
  EXPECT_TRUE(file_information.product_name.empty());
  EXPECT_TRUE(file_information.product_short_name.empty());
  EXPECT_TRUE(file_information.internal_name.empty());
  EXPECT_TRUE(file_information.original_filename.empty());
  EXPECT_TRUE(file_information.file_description.empty());
  EXPECT_TRUE(file_information.file_version.empty());
}

TEST(DiskUtilTests, RetrieveBasicFileInformationNoFile) {
  base::FilePath appdata_folder;
  ASSERT_TRUE(base::PathService::Get(CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA),
                                     &appdata_folder));

  base::FilePath non_existent_file(
      appdata_folder.DirName().Append(L"abcd1234CCT1234.tmp"));

  internal::FileInformation file_information;
  RetrieveBasicFileInformation(non_existent_file, &file_information);

  EXPECT_TRUE(file_information.path.empty());
  EXPECT_TRUE(file_information.creation_date.empty());
  EXPECT_TRUE(file_information.last_modified_date.empty());
  EXPECT_FALSE(file_information.active_file);
  EXPECT_TRUE(file_information.sha256.empty());
  EXPECT_EQ(0, file_information.size);
  EXPECT_TRUE(file_information.company_name.empty());
  EXPECT_TRUE(file_information.company_short_name.empty());
  EXPECT_TRUE(file_information.product_name.empty());
  EXPECT_TRUE(file_information.product_short_name.empty());
  EXPECT_TRUE(file_information.internal_name.empty());
  EXPECT_TRUE(file_information.original_filename.empty());
  EXPECT_TRUE(file_information.file_description.empty());
  EXPECT_TRUE(file_information.file_version.empty());
}

TEST(DiskUtilTests, FileInformationToString) {
  base::string16 display_str = FileInformationToString(kFileInformation1);
  EXPECT_EQ(kFileInformation1ExpectedString, display_str);
}

TEST(DiskUtilTests, FileInformationToStringEmpty) {
  internal::FileInformation file_information;
  EXPECT_TRUE(FileInformationToString(file_information).empty());
}

TEST(DiskUtilTests, TryToExpandPath_NonSystemNativePath) {
  base::ScopedPathOverride appdata_override(
      CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA));
  base::FilePath appdata_folder;
  ASSERT_TRUE(base::PathService::Get(CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA),
                                     &appdata_folder));

  base::FilePath non_existing_file(
      appdata_folder.DirName().Append(L"non-existing-file.tmp"));
  base::FilePath unused_expanded_path;
  ASSERT_FALSE(TryToExpandPath(non_existing_file, &unused_expanded_path));

  base::FilePath existing_file(appdata_folder.Append(L"existing-file.tmp"));
  CreateFileWithContent(existing_file, kFileContent1, sizeof(kFileContent1));
  base::FilePath expanded_path;
  ASSERT_TRUE(TryToExpandPath(existing_file, &expanded_path));
  ASSERT_EQ(existing_file.value(), expanded_path.value());

  // TODO: Figure out how to test paths in C:\Windows\System32.
}

TEST(DiskUtilTests, TruncateLogFileToTail_FindsNewline) {
  const char kFileContents[] = "File with utf8 \n\xe2\x82\xac \ntail";
  const int kFileSize = strlen(kFileContents);
  const char kTailContents[] = "\ntail";
  const int kTailSize = strlen(kTailContents);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().Append(L"file.txt");
  ASSERT_EQ(kFileSize, base::WriteFile(file_path, kFileContents, kFileSize));

  // The tail threshold lands in the middle of a multi-byte character.
  TruncateLogFileToTail(file_path, kTailSize + 3);

  int64_t file_size;
  ASSERT_TRUE(base::GetFileSize(file_path, &file_size));
  EXPECT_EQ(kTailSize, file_size);
  std::string tail;
  ASSERT_TRUE(base::ReadFileToString(file_path, &tail));
  EXPECT_EQ(kTailContents, tail);
}

TEST(DiskUtilTests, TruncateLogFileToTail_FileSmallerThanLimit) {
  const char kFileContents[] = "I am file";
  const int kFileSize = strlen(kFileContents);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().Append(L"file.txt");
  ASSERT_EQ(kFileSize, base::WriteFile(file_path, kFileContents, kFileSize));

  TruncateLogFileToTail(file_path, kFileSize * 2);

  int64_t file_size;
  ASSERT_TRUE(base::GetFileSize(file_path, &file_size));
  EXPECT_EQ(kFileSize, file_size);
  std::string tail;
  ASSERT_TRUE(base::ReadFileToString(file_path, &tail));
  EXPECT_EQ(kFileContents, tail);
}

TEST(DiskUtilTests, TruncateLogFileToTail_NotExisting) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().Append(L"file.txt");

  TruncateLogFileToTail(file_path, 42);

  EXPECT_FALSE(base::PathExists(file_path));
}

TEST(DiskUtilTests, IgnoredReportingList) {
  EXPECT_TRUE(IsCompanyOnIgnoredReportingList(L"Google LLC"));
  EXPECT_FALSE(IsCompanyOnIgnoredReportingList(L"google llc"));
  EXPECT_FALSE(IsCompanyOnIgnoredReportingList(L"Unrecognized Inc"));
}

}  // namespace chrome_cleaner
