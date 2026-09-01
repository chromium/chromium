// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/mini_installer.h"

#include <string>

#include "base/base_paths.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/install_static/install_details.h"
#include "chrome/installer/mini_installer/configuration.h"
#include "chrome/installer/mini_installer/mini_installer_constants.h"
#include "chrome/installer/mini_installer/path_string.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mini_installer {

namespace {

base::FilePath GetTestFileRootPath() {
  base::FilePath test_data_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_root);
  return test_data_root.Append(FILE_PATH_LITERAL("chrome"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .Append(FILE_PATH_LITERAL("installer"));
}

}  // namespace

TEST(MiniInstallerTest, AppendCommandLineFlags) {
  static constexpr struct {
    const wchar_t* command_line;
    const wchar_t* args;
  } kData[] = {
      {L"", L"foo.exe"},
      {L"mini_installer.exe", L"foo.exe"},
      {L"mini_installer.exe --verbose-logging", L"foo.exe --verbose-logging"},
      {L"C:\\Temp\\mini_installer.exe --verbose-logging",
       L"foo.exe --verbose-logging"},
      {L"C:\\Temp\\mini_installer --verbose-logging",
       L"foo.exe --verbose-logging"},
      {L"\"C:\\Temp\\mini_installer (1).exe\" --verbose-logging",
       L"foo.exe --verbose-logging"},
      {L"\"mini_installer.exe\"--verbose-logging",
       L"foo.exe --verbose-logging"},
  };

  CommandString buffer;

  for (const auto& data : kData) {
    buffer.assign(L"foo.exe");
    AppendCommandLineFlags(data.command_line, &buffer);
    EXPECT_STREQ(data.args, buffer.get()) << data.command_line;
  }
}

TEST(MiniInstallerTest, GetModuleDir) {
  PathString directory;

  ASSERT_TRUE(GetModuleDir(/*module=*/nullptr, &directory));
  ASSERT_NE(directory.length(), 0U);
  EXPECT_LT(directory.length(), directory.capacity());
  EXPECT_EQ(UNSAFE_TODO(directory.get()[directory.length() - 1]), L'\\');
}

struct UnpackParams {
  UnpackParams(base::FilePath mini_installer_file_path,
               std::wstring expected_unpacked_archive_file_name,
               const wchar_t* expected_setup_resource_type,
               const wchar_t* expected_archive_resource_type,
               bool is_compressed)
      : mini_installer_file_path(mini_installer_file_path),
        expected_unpacked_archive_file_name(
            expected_unpacked_archive_file_name),
        expected_setup_resource_type(expected_setup_resource_type),
        expected_archive_resource_type(expected_archive_resource_type),
        is_compressed(is_compressed) {}

  base::FilePath mini_installer_file_path;
  std::wstring expected_unpacked_archive_file_name;
  const wchar_t* expected_setup_resource_type;
  const wchar_t* expected_archive_resource_type;
  bool is_compressed;
};

class MiniInstallerTest : public ::testing::TestWithParam<UnpackParams> {};

INSTANTIATE_TEST_SUITE_P(CompressedArchive,
                         MiniInstallerTest,
                         ::testing::Values(UnpackParams(
                             GetTestFileRootPath().Append(
                                 FILE_PATH_LITERAL("mini_installer.exe.test")),
                             std::wstring(L"CHROME.PACKED.7Z"),
                             kLZCResourceType,
                             kLZMAResourceType,
                             true)));

INSTANTIATE_TEST_SUITE_P(
    UncompressedArchive,
    MiniInstallerTest,
    ::testing::Values(UnpackParams(
        GetTestFileRootPath().Append(
            FILE_PATH_LITERAL("mini_installer_uncompressed.exe.test")),
        std::wstring(L"CHROME.7Z"),
        kBinResourceType,
        kBinResourceType,
        false)));

// Tests unpacking setup.exe from a test mini_installer without writing archive
// to disk.
TEST_P(MiniInstallerTest, UnpackMiniInstaller) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  int max_delete_attempts = 0;
  PathString setup_path;
  PathString archive_name;
  ResourceTypeString archive_type;

  base::ScopedNativeLibrary loaded_module(
      LoadLibraryExW(GetParam().mini_installer_file_path.value().c_str(), NULL,
                     LOAD_LIBRARY_AS_IMAGE_RESOURCE));
  ASSERT_TRUE(loaded_module.is_valid());

  std::wstring temp_path = temp_dir.GetPath().value() + L"\\";
  ProcessExitResult exit_code =
      UnpackBinaryResources(loaded_module.get(), temp_path.c_str(), setup_path,
                            archive_name, archive_type, max_delete_attempts);
  EXPECT_EQ(exit_code.exit_code, SUCCESS_EXIT_CODE);

  base::FilePath expected_setup_path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("setup.exe"));
  EXPECT_STREQ(setup_path.get(), expected_setup_path.value().c_str());

  std::string actual_setup_data;
  EXPECT_TRUE(base::ReadFileToString(expected_setup_path, &actual_setup_data));
  EXPECT_STREQ(actual_setup_data.c_str(), "fakesetupdata");

  EXPECT_STREQ(archive_name.get(),
               GetParam().expected_unpacked_archive_file_name.c_str());
  EXPECT_STREQ(archive_type.get(), GetParam().expected_archive_resource_type);

  // The archive resource must NOT be written to disk.
  EXPECT_FALSE(base::PathExists(temp_dir.GetPath().Append(
      GetParam().expected_unpacked_archive_file_name)));

  if (GetParam().is_compressed) {
    EXPECT_TRUE(!base::PathExists(
        temp_dir.GetPath().Append(FILE_PATH_LITERAL("SETUP.EX_"))));
  } else {
    EXPECT_EQ(max_delete_attempts, 0);
  }
}

}  // namespace mini_installer
