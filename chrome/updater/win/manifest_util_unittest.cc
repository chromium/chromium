// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/manifest_util.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(ManifestUtil, ReadInstallCommandFromManifest) {
  const std::string app_id("{CDABE316-39CD-43BA-8440-6D1E0547AEE6}");
  const std::wstring manifest_filename(L"OfflineManifest.gup");
  const std::wstring executable_name(L"my_installer.exe");
  const std::wstring executable_name_v2(L"random_named_my_installer.exe");
  const std::wstring offline_dir_guid(
      L"{7B3A5597-DDEA-409B-B900-4C3D2A94A75C}");

  base::FilePath exe_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_dir));

  base::ScopedTempDir scoped_offline_base_dir;
  ASSERT_TRUE(scoped_offline_base_dir.Set(exe_dir.Append(L"Offline")));

  const base::FilePath offline_dir(
      scoped_offline_base_dir.GetPath().Append(offline_dir_guid));

  base::FilePath test_manifest;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_manifest));
  test_manifest = test_manifest.Append(L"updater").Append(manifest_filename);
  ASSERT_TRUE(base::PathExists(test_manifest));

  const base::FilePath offline_app_dir(offline_dir.AppendASCII(app_id));
  ASSERT_TRUE(base::CreateDirectory(offline_app_dir));
  ASSERT_TRUE(
      base::CopyFile(test_manifest, offline_dir.Append(manifest_filename)));

  const std::string dummy_file_contents("Test Executable Contents");
  const base::FilePath expected_installer_path(
      offline_app_dir.Append(executable_name));
  ASSERT_TRUE(base::WriteFile(expected_installer_path, dummy_file_contents));

  update_client::ProtocolParser::Results results;
  std::string installer_version;
  base::FilePath installer_path;
  std::string install_args;
  std::string install_data;

  ReadInstallCommandFromManifest(offline_dir_guid, app_id, "verboselogging",
                                 results, installer_version, installer_path,
                                 install_args, install_data);
  EXPECT_EQ(installer_version, "1.2.3.4");
  EXPECT_EQ(installer_path, expected_installer_path);
  EXPECT_EQ(install_args, "-baz");
  EXPECT_EQ(install_data,
            "{\n"
            "        \"distribution\": {\n"
            "          \"verbose_logging\": true\n"
            "        }\n"
            "      }");

  const base::FilePath expected_installer_path_v2(
      offline_app_dir.Append(executable_name_v2));
  ASSERT_TRUE(base::Move(expected_installer_path, expected_installer_path_v2));

  ReadInstallCommandFromManifest(offline_dir_guid, app_id, "verboselogging",
                                 results, installer_version, installer_path,
                                 install_args, install_data);
  EXPECT_EQ(installer_version, "1.2.3.4");
  EXPECT_EQ(installer_path, expected_installer_path_v2);
  EXPECT_EQ(install_args, "-baz");
  EXPECT_EQ(install_data,
            "{\n"
            "        \"distribution\": {\n"
            "          \"verbose_logging\": true\n"
            "        }\n"
            "      }");
}

struct ManifestUtilIsArchitectureSupportedTestCase {
  const std::string current_architecture;
  const std::string arch;
  const bool expected_result;
};

class ManifestUtilIsArchitectureSupportedTest
    : public ::testing::TestWithParam<
          ManifestUtilIsArchitectureSupportedTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    ManifestUtilIsArchitectureSupportedTestCases,
    ManifestUtilIsArchitectureSupportedTest,
    ::testing::ValuesIn([] {
      std::vector<ManifestUtilIsArchitectureSupportedTestCase> test_cases;
      for (const std::string current_architecture :
           {update_client::kArchIntel, update_client::kArchAmd64,
            update_client::kArchArm64}) {
        test_cases.push_back({current_architecture, "", true});
        test_cases.push_back({current_architecture, "unknown", false});
        test_cases.push_back({current_architecture, "x86", true});
        test_cases.push_back(
            {current_architecture, "x64",
             current_architecture == update_client::kArchAmd64});
        test_cases.push_back(
            {current_architecture, "x86_64",
             current_architecture == update_client::kArchAmd64});
      }
      return test_cases;
    }()));

TEST_P(ManifestUtilIsArchitectureSupportedTest, TestCases) {
  EXPECT_EQ(
      IsArchitectureSupported(GetParam().arch, GetParam().current_architecture),
      GetParam().expected_result)
      << GetParam().arch << ": " << GetParam().current_architecture << ": "
      << GetParam().expected_result;
}

TEST(ManifestUtil, IsPlatformCompatible) {
  EXPECT_TRUE(IsPlatformCompatible({}));
  EXPECT_TRUE(IsPlatformCompatible("win"));
  EXPECT_FALSE(IsPlatformCompatible("mac"));
}

struct ManifestUtilIsArchitectureCompatibleTestCase {
  const std::string current_architecture;
  const std::string arch_list;
  const bool expected_result;
};

class ManifestUtilIsArchitectureCompatibleTest
    : public ::testing::TestWithParam<
          ManifestUtilIsArchitectureCompatibleTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    ManifestUtilIsArchitectureCompatibleTestCases,
    ManifestUtilIsArchitectureCompatibleTest,
    ::testing::ValuesIn([] {
      std::vector<ManifestUtilIsArchitectureCompatibleTestCase> test_cases;
      for (const std::string current_architecture :
           {update_client::kArchIntel, update_client::kArchAmd64,
            update_client::kArchArm64}) {
        test_cases.push_back({current_architecture, "", true});
        test_cases.push_back({current_architecture, "unknown", false});
        test_cases.push_back({current_architecture, "x86", true});
        test_cases.push_back(
            {current_architecture, "x64",
             current_architecture == update_client::kArchAmd64});
        test_cases.push_back(
            {current_architecture, "-x64",
             current_architecture != update_client::kArchAmd64});
        test_cases.push_back(
            {current_architecture, "-x86_64",
             current_architecture != update_client::kArchAmd64});
        test_cases.push_back(
            {current_architecture, "-x86",
             current_architecture != update_client::kArchIntel});
        test_cases.push_back(
            {current_architecture, "x86,-x64",
             current_architecture != update_client::kArchAmd64});
        test_cases.push_back(
            {current_architecture, "x86,x64,-arm64",
             current_architecture != update_client::kArchArm64});
      }
      return test_cases;
    }()));

TEST_P(ManifestUtilIsArchitectureCompatibleTest, TestCases) {
  EXPECT_EQ(IsArchitectureCompatible(GetParam().arch_list,
                                     GetParam().current_architecture),
            GetParam().expected_result)
      << GetParam().arch_list << ": " << GetParam().current_architecture << ": "
      << GetParam().expected_result;
}

TEST(ManifestUtil, IsOSVersionCompatible) {
  EXPECT_TRUE(IsOSVersionCompatible({}));
  EXPECT_TRUE(IsOSVersionCompatible("6.0"));
  EXPECT_FALSE(IsOSVersionCompatible("60.0"));
  EXPECT_TRUE(IsOSVersionCompatible("0.1"));
  EXPECT_FALSE(IsOSVersionCompatible("foobar"));
}

struct ManifestUtilIsOsSupportedTestCase {
  const std::string platform;
  const std::string arch_list;
  const std::string min_os_version;
  const bool expected_result;
};

class ManifestUtilIsOsSupportedTest
    : public ::testing::TestWithParam<ManifestUtilIsOsSupportedTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    ManifestUtilIsOsSupportedTestCases,
    ManifestUtilIsOsSupportedTest,
    ::testing::ValuesIn(std::vector<ManifestUtilIsOsSupportedTestCase>{
        {"win", "x86", "6.0", true},
        {"mac", "x86", "6.0", false},
        {"win", "unknown", "6.0", false},
        {"win", "x64", "6.0",
         update_client::GetArchitecture() == update_client::kArchAmd64},
        {"win", "-x64", "6.0",
         update_client::GetArchitecture() != update_client::kArchAmd64},
        {"win", "x86,-x64", "6.0",
         update_client::GetArchitecture() != update_client::kArchAmd64},
        {"win", "x86,x64,-arm64", "6.0",
         update_client::GetArchitecture() != update_client::kArchArm64},
        {"win", "x86", "60.0", false},
        {"win", "x86", "0.01", true},
    }));

TEST_P(ManifestUtilIsOsSupportedTest, TestCases) {
  update_client::ProtocolParser::Results results;
  update_client::ProtocolParser::SystemRequirements& system_requirements =
      results.system_requirements;
  system_requirements.platform = GetParam().platform;
  system_requirements.arch = GetParam().arch_list;
  system_requirements.min_os_version = GetParam().min_os_version;

  EXPECT_EQ(IsOsSupported(results), GetParam().expected_result)
      << GetParam().platform << ": " << GetParam().arch_list << ": "
      << GetParam().min_os_version << ": " << update_client::GetArchitecture()
      << ": " << GetParam().expected_result;
}

}  // namespace updater
