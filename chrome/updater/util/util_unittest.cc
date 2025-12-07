// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/util.h"

#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

void SetupMockDirectory(const base::FilePath& mock_dir,
                        const base::FilePath::StringType& mock_extension) {
  for (const base::FilePath& dir :
       {mock_dir, mock_dir.Append(FILE_PATH_LITERAL("SubDir1")),
        mock_dir.Append(FILE_PATH_LITERAL("SubDir2"))}) {
    ASSERT_TRUE(base::CreateDirectory(dir));
    base::FilePath temp_file;
    ASSERT_TRUE(base::CreateTemporaryFileInDir(dir, &temp_file));
    if (mock_extension[0]) {
      ASSERT_TRUE(
          base::CopyFile(temp_file, temp_file.AddExtension(mock_extension)));
    }
  }
}

}  // namespace

struct UtilTagArgsTestCase {
  const std::string tag_switch;
};

class UtilTagArgsTest : public ::testing::TestWithParam<UtilTagArgsTestCase> {};

INSTANTIATE_TEST_SUITE_P(UtilTagArgsTestCases,
                         UtilTagArgsTest,
                         ::testing::ValuesIn(std::vector<UtilTagArgsTestCase>{
                             {kInstallSwitch},
                             {kHandoffSwitch},
                         }));

TEST_P(UtilTagArgsTest, AppArgsAndAP) {
  base::test::ScopedCommandLine original_command_line;
  {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchUTF8(GetParam().tag_switch,
                                   "appguid=8a69f345-c564-463c-aff1-"
                                   "a69d9e530f96&appname=TestApp&ap=TestAP");

    // Test GetAppArgs.
    EXPECT_EQ(GetAppArgs("NonExistentAppId"), std::nullopt);
    std::optional<tagging::AppArgs> app_args =
        GetAppArgs("8a69f345-c564-463c-aff1-a69d9e530f96");
    ASSERT_NE(app_args, std::nullopt);
    EXPECT_EQ(app_args->app_id, "8a69f345-c564-463c-aff1-a69d9e530f96");
    EXPECT_EQ(app_args->app_name, "TestApp");
  }
}

TEST_P(UtilTagArgsTest, GetTagArgsForCommandLine) {
  base::CommandLine command_line(base::FilePath(FILE_PATH_LITERAL("my.exe")));
  command_line.AppendSwitchUTF8(GetParam().tag_switch,
                                "appguid={8a69}&appname=Chrome");
  command_line.AppendSwitchUTF8(kAppArgsSwitch,
                                "&appguid={8a69}&installerdata=%7B%22homepage%"
                                "22%3A%22http%3A%2F%2Fwww.google.com%");
  command_line.AppendSwitch(kSilentSwitch);
  command_line.AppendSwitchUTF8(kSessionIdSwitch, "{123-456}");

  TagParsingResult result = GetTagArgsForCommandLine(command_line);
  EXPECT_EQ(result.error, tagging::ErrorCode::kSuccess);
  EXPECT_EQ(result.tag_args->apps.size(), size_t{1});
  EXPECT_EQ(result.tag_args->apps[0].app_id, "{8a69}");
  EXPECT_EQ(result.tag_args->apps[0].app_name, "Chrome");
  EXPECT_EQ(result.tag_args->apps[0].encoded_installer_data,
            "%7B%22homepage%22%3A%22http%3A%2F%2Fwww.google.com%");
}

TEST(Util, WriteInstallerDataToTempFile) {
  base::FilePath directory;
  ASSERT_TRUE(base::PathService::Get(base::DIR_MODULE, &directory));

  EXPECT_FALSE(WriteInstallerDataToTempFile(directory, ""));

  const std::string kInstallerData =
      R"({"distribution":{"msi":true,"allow_downgrade":false}})";
  EXPECT_FALSE(WriteInstallerDataToTempFile(
      directory.Append(FILE_PATH_LITERAL("NonExistentDirectory")),
      kInstallerData));

  const std::optional<base::FilePath> installer_data_file =
      WriteInstallerDataToTempFile(directory, kInstallerData);
  ASSERT_TRUE(installer_data_file);

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(*installer_data_file, &contents));
  EXPECT_EQ(base::StrCat({kUTF8BOM, kInstallerData}), contents);

  EXPECT_TRUE(base::DeleteFile(*installer_data_file));
}

TEST(Util, GetCrashDatabasePath) {
  std::optional<base::FilePath> crash_database_path(
      GetCrashDatabasePath(GetUpdaterScopeForTesting()));
  ASSERT_TRUE(crash_database_path);
  EXPECT_EQ(crash_database_path->BaseName().value(),
            FILE_PATH_LITERAL("Crashpad"));
}

TEST(Util, GetCrxCacheDirectory) {
  std::optional<base::FilePath> diff_cache_directory(
      GetCrxCacheDirectory(GetUpdaterScopeForTesting()));
  ASSERT_TRUE(diff_cache_directory);
  EXPECT_EQ(diff_cache_directory->BaseName().value(),
            FILE_PATH_LITERAL("crx_cache"));
}

TEST(Util, StreamEnumValue) {
  enum class TestEnum {
    kValue1 = 0L,
    kValue2 = 5L,
    kValue3,
  };

  std::stringstream output;
  output << "First: " << TestEnum::kValue1 << ", second: " << TestEnum::kValue2
         << ", third: " << TestEnum::kValue3;
  EXPECT_EQ(output.str(), "First: 0, second: 5, third: 6");
}

TEST(Util, DeleteExcept) {
  EXPECT_FALSE(DeleteExcept({}));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath except_executable(temp_dir.GetPath().Append(
      FILE_PATH_LITERAL("except_executable.executable")));
  test::SetupMockUpdater(except_executable);
  EXPECT_TRUE(DeleteExcept(except_executable));
  test::ExpectOnlyMockUpdater(except_executable);
}

TEST(Util, CeilingDivide) {
  EXPECT_EQ(CeilingDivide(0, 1), 0);
  EXPECT_EQ(CeilingDivide(1, 2), 1);
  EXPECT_EQ(CeilingDivide(1, 1), 1);
  EXPECT_EQ(CeilingDivide(3, 2), 2);
  EXPECT_EQ(CeilingDivide(5, 3), 2);
  EXPECT_EQ(CeilingDivide(4, 2), 2);

  EXPECT_EQ(CeilingDivide(-1, 2), 0);
  EXPECT_EQ(CeilingDivide(-1, 1), -1);
  EXPECT_EQ(CeilingDivide(-3, 2), -1);
  EXPECT_EQ(CeilingDivide(-5, 3), -1);
  EXPECT_EQ(CeilingDivide(-2, 1), -2);
  EXPECT_EQ(CeilingDivide(-4, 2), -2);

  EXPECT_EQ(CeilingDivide(1, -2), 0);
  EXPECT_EQ(CeilingDivide(1, -1), -1);
  EXPECT_EQ(CeilingDivide(3, -2), -1);
  EXPECT_EQ(CeilingDivide(5, -3), -1);
  EXPECT_EQ(CeilingDivide(2, -1), -2);
  EXPECT_EQ(CeilingDivide(4, -2), -2);

  EXPECT_EQ(CeilingDivide(-0, -1), 0);
  EXPECT_EQ(CeilingDivide(-1, -2), 1);
  EXPECT_EQ(CeilingDivide(-1, -1), 1);
  EXPECT_EQ(CeilingDivide(-3, -2), 2);
  EXPECT_EQ(CeilingDivide(-5, -3), 2);
  EXPECT_EQ(CeilingDivide(-4, -2), 2);
}

TEST(Util, OptionalBaseInsertion) {
  // Tests insertion in a gTest expectation.
  std::optional<base::FilePath> file_path;
  EXPECT_TRUE(true) << file_path;

  std::stringstream os;
  os << file_path << std::endl;
  EXPECT_EQ(os.str(), "std::nullopt\n");
  os.str("");
  file_path = std::make_optional<base::FilePath>(FILE_PATH_LITERAL("test"));
  os << file_path << std::endl;
  EXPECT_EQ(os.str(), "test\n");
}

TEST(WinUtil, GetDownloadProgress) {
  EXPECT_EQ(GetDownloadProgress(0, 50), 0);
  EXPECT_EQ(GetDownloadProgress(12, 50), 24);
  EXPECT_EQ(GetDownloadProgress(25, 50), 50);
  EXPECT_EQ(GetDownloadProgress(50, 50), 100);
  EXPECT_EQ(GetDownloadProgress(50, 50), 100);
  EXPECT_EQ(GetDownloadProgress(0, -1), -1);
  EXPECT_EQ(GetDownloadProgress(-1, -1), -1);
  EXPECT_EQ(GetDownloadProgress(50, 0), -1);
}

TEST(Util, ToSignedIntegral) {
  EXPECT_EQ(ToSignedIntegral(uint8_t{0}), 0);
  EXPECT_EQ(ToSignedIntegral(uint8_t{0x7F}), 0x7F);
  EXPECT_EQ(ToSignedIntegral(uint8_t{0x80}), -1);
  EXPECT_EQ(ToSignedIntegral(uint32_t{0}), 0);
  EXPECT_EQ(ToSignedIntegral(uint32_t{1357}), 1357);
  EXPECT_EQ(ToSignedIntegral(uint32_t{0x7FFFFFFF}), 0x7FFFFFFF);
  EXPECT_EQ(ToSignedIntegral(uint32_t{0x80000000}), -1);
  EXPECT_EQ(ToSignedIntegral(uint32_t{0xFFFFFFFF}), -1);
  EXPECT_EQ(ToSignedIntegral(uint64_t{0x7FFFFFFFFFFFFFFF}), 0x7FFFFFFFFFFFFFFF);
  EXPECT_EQ(ToSignedIntegral(uint64_t{0x8000000000000000}), -1);
}

#if BUILDFLAG(IS_WIN)
class UtilTaskNameTest : public ::testing::TestWithParam<std::string> {
 protected:
  base::Version version() const { return base::Version(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(UtilTaskNameTestCases,
                         UtilTaskNameTest,
                         ::testing::Values(kUpdaterVersion,
                                           "1.2.3.4",
                                           "199.28537.11717"));

TEST_P(UtilTaskNameTest, GetTaskNamePrefix) {
  EXPECT_EQ(
      GetTaskNamePrefix(GetUpdaterScopeForTesting(), version()),
      base::StrCat(
          {base::UTF8ToWide(PRODUCT_FULLNAME_STRING), L"Task",
           IsSystemInstall(GetUpdaterScopeForTesting()) ? L"System" : L"User",
           base::UTF8ToWide(version().GetString())}));
}

TEST_P(UtilTaskNameTest, GetTaskDisplayName) {
  EXPECT_EQ(
      GetTaskDisplayName(GetUpdaterScopeForTesting(), version()),
      base::StrCat(
          {base::UTF8ToWide(PRODUCT_FULLNAME_STRING), L" Task ",
           IsSystemInstall(GetUpdaterScopeForTesting()) ? L"System " : L"User ",
           base::UTF8ToWide(version().GetString())}));
}
#endif  // BUILDFLAG(IS_WIN)

TEST(Util, GetFilesWithPredicate) {
  EXPECT_TRUE(GetFilesWithPredicate({}, [](const base::FilePath&) {
                return true;
              }).empty());

  for (const auto& extension :
       {FILE_PATH_LITERAL(".log"), FILE_PATH_LITERAL("")}) {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    ASSERT_NO_FATAL_FAILURE(SetupMockDirectory(temp_dir.GetPath(), extension));
    EXPECT_EQ(
        GetFilesWithPredicate(temp_dir.GetPath(),
                              [&](const base::FilePath& item) {
                                return item.MatchesFinalExtension(extension);
                              })
            .size(),
        extension[0] ? 3u : 0u);
  }
}

TEST(Util, EnumerateUpdateClientTempDirectories) {
  ASSERT_NO_FATAL_FAILURE(EnumerateUpdateClientTempDirectories(
      GetUpdaterScopeForTesting(), [](const base::FilePath& dir) {
        ADD_FAILURE() << "Unexpected directory: " << dir;
      }));

  base::FilePath dir;
  for (const auto& matcher :
       {FILE_PATH_LITERAL("chrome_url_fetcher_"),
        FILE_PATH_LITERAL("chrome_Unpacker_BeginUnzipping"),
        FILE_PATH_LITERAL("chrome_BITS_"), FILE_PATH_LITERAL("BazBar")}) {
    ASSERT_TRUE(base::CreateNewTempDirectory(matcher, &dir));
  }

  int count = 0;
  ASSERT_NO_FATAL_FAILURE(EnumerateUpdateClientTempDirectories(
      GetUpdaterScopeForTesting(), [&count](const base::FilePath& dir) {
        ++count;
        EXPECT_TRUE(base::DeletePathRecursively(dir));
      }));

  EXPECT_EQ(count, 3);
  EXPECT_TRUE(base::DeletePathRecursively(dir));
}

}  // namespace updater
