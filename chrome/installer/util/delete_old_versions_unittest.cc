// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/delete_old_versions.h"

#include <set>
#include <string>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "chrome/installer/test/alternate_version_generator.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using upgrade_test::Direction;

namespace installer {

namespace {

class DeleteOldVersionsTest : public testing::Test {
 public:
  DeleteOldVersionsTest(const DeleteOldVersionsTest&) = delete;
  DeleteOldVersionsTest& operator=(const DeleteOldVersionsTest&) = delete;

 protected:
  DeleteOldVersionsTest() = default;

  void SetUp() override { ASSERT_TRUE(install_dir_.CreateUniqueTempDir()); }

  // Creates a copy of the current executable with a distinct version of name
  // |name| in |install_dir_|. Depending on |direction|, the version of the
  // created executable is higher or lower than the version of the current
  // executable.
  std::wstring CreateExecutable(const std::wstring& name, Direction direction) {
    base::FilePath current_exe_path;
    if (!base::PathService::Get(base::FILE_EXE, &current_exe_path))
      return std::wstring();

    return upgrade_test::GenerateAlternatePEFileVersion(
        current_exe_path, install_dir().Append(name), direction);
  }

  // Creates in |install_dir_| a directory named |name| containing a subset of
  // dummy files impersonating a Chrome version directory.
  bool CreateVersionDirectory(const std::wstring& name) {
    static constexpr char kDummyContent[] = "dummy";
    const base::FilePath version_dir_path(install_dir().Append(name));

    return base::CreateDirectory(install_dir().Append(name)) &&
           base::CreateDirectory(version_dir_path.Append(L"Installer")) &&
           base::WriteFile(version_dir_path.Append(L"chrome.dll"),
                           kDummyContent) &&
           base::WriteFile(version_dir_path.Append(L"icudtl.dat"),
                           kDummyContent) &&
           base::WriteFile(version_dir_path.Append(L"Installer\\setup.exe"),
                           kDummyContent);
  }

  // Returns the relative paths of all files and directories in |install_dir_|.
  using FilePathSet = std::set<base::FilePath>;
  FilePathSet GetInstallDirContent() const {
    std::set<base::FilePath> content;
    base::FileEnumerator file_enumerator(
        install_dir(), true,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    for (base::FilePath path = file_enumerator.Next(); !path.empty();
         path = file_enumerator.Next()) {
      DCHECK(base::StartsWith(path.value(), install_dir().value(),
                              base::CompareCase::SENSITIVE));
      content.insert(base::FilePath(
          path.value().substr(install_dir().value().size() + 1)));
    }
    return content;
  }

  // Adds to |file_path_set| all files and directories that are expected to be
  // found in the version directory |version| before any attempt to delete it.
  void AddVersionFiles(const std::wstring& version,
                       FilePathSet* file_path_set) {
    file_path_set->insert(base::FilePath(version));
    file_path_set->insert(base::FilePath(version).Append(L"chrome.dll"));
    file_path_set->insert(base::FilePath(version).Append(L"icudtl.dat"));
    file_path_set->insert(base::FilePath(version).Append(L"Installer"));
    file_path_set->insert(
        base::FilePath(version).Append(L"Installer\\setup.exe"));
  }

  base::FilePath install_dir() const { return install_dir_.GetPath(); }

 private:
  base::ScopedTempDir install_dir_;
};

}  // namespace

// An old executable without a matching directory should be deleted.
TEST_F(DeleteOldVersionsTest, DeleteOldExecutableWithoutMatchingDirectory) {
  ASSERT_FALSE(
      CreateExecutable(installer::kChromeOldExe, Direction::PREVIOUS_VERSION)
          .empty());

  DeleteOldVersions(install_dir());
  EXPECT_TRUE(GetInstallDirContent().empty());
}

// chrome.exe and new_chrome.exe should never be deleted.
TEST_F(DeleteOldVersionsTest, DeleteNewExecutablesWithoutMatchingDirectory) {
  ASSERT_FALSE(
      CreateExecutable(installer::kChromeExe, Direction::PREVIOUS_VERSION)
          .empty());
  ASSERT_FALSE(
      CreateExecutable(installer::kChromeNewExe, Direction::NEXT_VERSION)
          .empty());

  DeleteOldVersions(install_dir());
  FilePathSet expected_install_dir_content;
  expected_install_dir_content.insert(base::FilePath(installer::kChromeExe));
  expected_install_dir_content.insert(base::FilePath(installer::kChromeNewExe));
  EXPECT_EQ(expected_install_dir_content, GetInstallDirContent());
}

// A directory without a matching executable should be deleted.
TEST_F(DeleteOldVersionsTest, DeleteDirectoryWithoutMatchingExecutable) {
  static constexpr wchar_t kVersion[] = L"48.0.0.0";
  ASSERT_TRUE(CreateVersionDirectory(kVersion));

  DeleteOldVersions(install_dir());
  EXPECT_TRUE(GetInstallDirContent().empty());
}

// A pair of matching old executable/version directory that is not in use should
// be deleted.
TEST_F(DeleteOldVersionsTest, DeleteOldExecutableWithMatchingDirectory) {
  const std::wstring version_a =
      CreateExecutable(installer::kChromeOldExe, Direction::PREVIOUS_VERSION);
  ASSERT_FALSE(version_a.empty());
  ASSERT_TRUE(CreateVersionDirectory(version_a));

  DeleteOldVersions(install_dir());
  EXPECT_TRUE(GetInstallDirContent().empty());
}

// chrome.exe, new_chrome.exe and their matching version directories should
// never be deleted.
TEST_F(DeleteOldVersionsTest, DeleteNewExecutablesWithMatchingDirectory) {
  const std::wstring version_a =
      CreateExecutable(installer::kChromeExe, Direction::PREVIOUS_VERSION);
  ASSERT_FALSE(version_a.empty());
  ASSERT_TRUE(CreateVersionDirectory(version_a));
  const std::wstring version_b =
      CreateExecutable(installer::kChromeNewExe, Direction::NEXT_VERSION);
  ASSERT_FALSE(version_b.empty());
  ASSERT_TRUE(CreateVersionDirectory(version_b));

  DeleteOldVersions(install_dir());

  FilePathSet expected_install_dir_content;
  expected_install_dir_content.insert(base::FilePath(installer::kChromeExe));
  AddVersionFiles(version_a, &expected_install_dir_content);
  expected_install_dir_content.insert(base::FilePath(installer::kChromeNewExe));
  AddVersionFiles(version_b, &expected_install_dir_content);
  EXPECT_EQ(expected_install_dir_content, GetInstallDirContent());
}

// chrome.exe, new_chrome.exe and their matching version directories should
// never be deleted, even when files named old_chrome*.exe have the same
// versions as chrome.exe/new_chrome.exe. The old_chrome*.exe files, however,
// should be deleted.
TEST_F(DeleteOldVersionsTest,
       DeleteNewExecutablesWithMatchingDirectoryAndOldExecutables) {
  const std::wstring version_a =
      CreateExecutable(installer::kChromeExe, Direction::PREVIOUS_VERSION);
  ASSERT_FALSE(version_a.empty());
  ASSERT_TRUE(CreateVersionDirectory(version_a));
  const std::wstring version_b =
      CreateExecutable(installer::kChromeNewExe, Direction::NEXT_VERSION);
  ASSERT_FALSE(version_b.empty());
  ASSERT_TRUE(CreateVersionDirectory(version_b));
  EXPECT_EQ(version_a,
            CreateExecutable(L"old_chrome.exe", Direction::PREVIOUS_VERSION));
  EXPECT_EQ(version_b,
            CreateExecutable(L"old_chrome2.exe", Direction::NEXT_VERSION));

  DeleteOldVersions(install_dir());

  FilePathSet expected_install_dir_content;
  expected_install_dir_content.insert(base::FilePath(installer::kChromeExe));
  AddVersionFiles(version_a, &expected_install_dir_content);
  expected_install_dir_content.insert(base::FilePath(installer::kChromeNewExe));
  AddVersionFiles(version_b, &expected_install_dir_content);
  EXPECT_EQ(expected_install_dir_content, GetInstallDirContent());
}

// No file should be deleted for a given version if the executable is in use.
TEST_F(DeleteOldVersionsTest, DeleteVersionWithExecutableInUse) {
  const std::wstring version_a =
      CreateExecutable(installer::kChromeOldExe, Direction::PREVIOUS_VERSION);
  ASSERT_FALSE(version_a.empty());
  ASSERT_TRUE(CreateVersionDirectory(version_a));

  base::File file_in_use(install_dir().Append(installer::kChromeOldExe),
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file_in_use.IsValid());

  DeleteOldVersions(install_dir());

  FilePathSet expected_install_dir_content;
  expected_install_dir_content.insert(base::FilePath(installer::kChromeOldExe));
  AddVersionFiles(version_a, &expected_install_dir_content);
  EXPECT_EQ(expected_install_dir_content, GetInstallDirContent());
}

// No file should be deleted for a given version if a .dll file in the version
// directory is in use.
TEST_F(DeleteOldVersionsTest, DeleteVersionWithVersionDirectoryDllInUse) {
  const std::wstring version_a =
      CreateExecutable(installer::kChromeOldExe, Direction::PREVIOUS_VERSION);
  ASSERT_FALSE(version_a.empty());
  ASSERT_TRUE(CreateVersionDirectory(version_a));

  base::File file_in_use(install_dir().Append(version_a).Append(L"chrome.dll"),
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file_in_use.IsValid());

  DeleteOldVersions(install_dir());

  FilePathSet expected_install_dir_content;
  expected_install_dir_content.insert(base::FilePath(installer::kChromeOldExe));
  AddVersionFiles(version_a, &expected_install_dir_content);
  EXPECT_EQ(expected_install_dir_content, GetInstallDirContent());
}

// No file should be deleted for a given version if a .exe file in the version
// directory is in use.
TEST_F(DeleteOldVersionsTest, DeleteVersionWithVersionDirectoryExeInUse) {
  const std::wstring version_a =
      CreateExecutable(installer::kChromeOldExe, Direction::PREVIOUS_VERSION);
  ASSERT_FALSE(version_a.empty());
  ASSERT_TRUE(CreateVersionDirectory(version_a));

  base::File file_in_use(
      install_dir().Append(version_a).Append(L"Installer\\setup.exe"),
      base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file_in_use.IsValid());

  DeleteOldVersions(install_dir());

  FilePathSet expected_install_dir_content;
  expected_install_dir_content.insert(base::FilePath(installer::kChromeOldExe));
  AddVersionFiles(version_a, &expected_install_dir_content);
  EXPECT_EQ(expected_install_dir_content, GetInstallDirContent());
}

// If an installation directory contains a file named chrome.exe with a matching
// directory v1 and a file named old_chrome.exe with a matching directory v2,
// old_chrome.exe and v2 should be deleted but chrome.exe and v1 shouldn't.
TEST_F(DeleteOldVersionsTest, TypicalAfterRenameState) {
  const std::wstring version_a =
      CreateExecutable(installer::kChromeOldExe, Direction::PREVIOUS_VERSION);
  ASSERT_FALSE(version_a.empty());
  ASSERT_TRUE(CreateVersionDirectory(version_a));
  const std::wstring version_b =
      CreateExecutable(installer::kChromeExe, Direction::NEXT_VERSION);
  ASSERT_FALSE(version_b.empty());
  ASSERT_TRUE(CreateVersionDirectory(version_b));

  DeleteOldVersions(install_dir());

  FilePathSet expected_install_dir_content;
  expected_install_dir_content.insert(base::FilePath(installer::kChromeExe));
  AddVersionFiles(version_b, &expected_install_dir_content);
  EXPECT_EQ(expected_install_dir_content, GetInstallDirContent());
}

}  // namespace installer
