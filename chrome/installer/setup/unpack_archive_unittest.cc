// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/unpack_archive.h"

#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/types/expected.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

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

class FakeInstallerState : public InstallerState {
 public:
  FakeInstallerState() { set_level(Level::SYSTEM_LEVEL); }
};

struct UnpackParams {
  UnpackParams(base::FilePath test_file,
               std::string archive_switch,
               bool uncompressed_output_matches_input_file)
      : test_file(test_file),
        archive_switch(archive_switch),
        uncompressed_output_matches_input_file(
            uncompressed_output_matches_input_file) {}

  base::FilePath test_file;
  std::string archive_switch;
  bool uncompressed_output_matches_input_file;
};

class SetupUnpackArchiveTest : public testing::TestWithParam<UnpackParams> {};

INSTANTIATE_TEST_SUITE_P(UnpackCompressedArchive,
                         SetupUnpackArchiveTest,
                         ::testing::Values(UnpackParams(
                             GetTestFileRootPath().Append(
                                 FILE_PATH_LITERAL("test_chrome.packed.7z")),
                             "install-archive",
                             false)));

// In this case, since the uncompressed archive was passed directly, the input
// archive path should match the output archive path.
INSTANTIATE_TEST_SUITE_P(
    UnpackUncompressedArchive,
    SetupUnpackArchiveTest,
    ::testing::Values(UnpackParams(
        GetTestFileRootPath().Append(FILE_PATH_LITERAL("test_chrome.7z")),
        "uncompressed-archive",
        true)));

// Tests that the setup is able to decompress chrome.packed.7z and unpack the
// chrome.7z archive it contains, when specified via the `--install-archive`
// flag.
TEST_P(SetupUnpackArchiveTest, UnpackArchive) {
  base::FilePath chrome_archive = GetParam().test_file;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  InstallationState original_state;  // Unused when not patching.
  base::CommandLine cmd_line = base::CommandLine::FromString(L"setup.exe");
  cmd_line.AppendSwitchPath(GetParam().archive_switch, chrome_archive);
  FakeInstallerState installer_state;
  ArchiveType archive_type;
  base::FilePath uncompressed_chrome_archive_out;

  base::expected<void, InstallStatus> result = UnpackAndMaybePatchChromeArchive(
      temp_dir.GetPath(), original_state,
      base::FilePath(),  // Unused when archive is provided via cmd_line.
      cmd_line, installer_state, &archive_type,
      uncompressed_chrome_archive_out);
  ASSERT_TRUE(result.has_value()) << result.error();

  // Instead of containing "chrome-bin", the test archives contain this
  // "test_data.txt". Make sure the output file exists and matches the test
  // data.
  std::string actual_installer_data;
  EXPECT_TRUE(base::ReadFileToString(base::FilePath(temp_dir.GetPath().Append(
                                         FILE_PATH_LITERAL("test_data.txt"))),
                                     &actual_installer_data));
  EXPECT_STREQ(actual_installer_data.c_str(), "fakechromiumdata");

  EXPECT_EQ(uncompressed_chrome_archive_out,
            GetParam().uncompressed_output_matches_input_file
                ? chrome_archive
                : temp_dir.GetPath().Append(FILE_PATH_LITERAL("chrome.7z")));
  ASSERT_EQ(archive_type, ArchiveType::FULL_ARCHIVE_TYPE);
}

TEST(SetupUnpackArchiveTest, UnpackFailsWhenCompressedAndUncompressedProvided) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  InstallationState original_state;  // Unused when not patching.
  base::CommandLine cmd_line = base::CommandLine::FromString(L"setup.exe");
  cmd_line.AppendSwitchPath(
      "install-archive",
      GetTestFileRootPath().Append(FILE_PATH_LITERAL("test_chrome.packed.7z")));
  cmd_line.AppendSwitchPath(
      "uncompressed-archive",
      GetTestFileRootPath().Append(FILE_PATH_LITERAL("test_chrome.7z")));
  FakeInstallerState installer_state;
  ArchiveType archive_type;
  base::FilePath uncompressed_chrome_archive_out;

  base::expected<void, InstallStatus> result = UnpackAndMaybePatchChromeArchive(
      temp_dir.GetPath(), original_state,
      base::FilePath(),  // Unused when archive is provided via cmd_line.
      cmd_line, installer_state, &archive_type,
      uncompressed_chrome_archive_out);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error(), UNSUPPORTED_OPTION);
}

}  // namespace installer
