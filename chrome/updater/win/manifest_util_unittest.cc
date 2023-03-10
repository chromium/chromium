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

TEST(ManifestUtil, IsArchitectureSupported) {
  for (const char* const current_architecture :
       {update_client::kArchIntel, update_client::kArchAmd64,
        update_client::kArchArm64}) {
    const struct {
      const char* arch;
      const bool expected_result;
    } test_cases[] = {
        {"", true},
        {"unknown", false},
        {"x86", true},
        {"x64", current_architecture == update_client::kArchAmd64},
        {"x86_64", current_architecture == update_client::kArchAmd64},
    };

    for (const auto& test_case : test_cases) {
      EXPECT_EQ(IsArchitectureSupported(test_case.arch, current_architecture),
                test_case.expected_result)
          << test_case.arch << ": " << current_architecture << ": "
          << test_case.expected_result;
    }
  }
}

TEST(ManifestUtil, IsPlatformCompatible) {
  EXPECT_TRUE(IsPlatformCompatible({}));
  EXPECT_TRUE(IsPlatformCompatible("win"));
  EXPECT_FALSE(IsPlatformCompatible("mac"));
}

TEST(ManifestUtil, IsArchitectureCompatible) {
  for (const char* const current_architecture :
       {update_client::kArchIntel, update_client::kArchAmd64,
        update_client::kArchArm64}) {
    const struct {
      const char* arch_list;
      const bool expected_result;
    } test_cases[] = {
        {"", true},
        {"unknown", false},
        {"x86", true},
        {"x64", current_architecture == update_client::kArchAmd64},
        {"-x64", current_architecture != update_client::kArchAmd64},
        {"-x86_64", current_architecture != update_client::kArchAmd64},
        {"-x86", current_architecture != update_client::kArchIntel},
        {"x86,-x64", current_architecture != update_client::kArchAmd64},
        {"x86,x64,-arm64", current_architecture != update_client::kArchArm64},
    };

    for (const auto& test_case : test_cases) {
      EXPECT_EQ(
          IsArchitectureCompatible(test_case.arch_list, current_architecture),
          test_case.expected_result)
          << test_case.arch_list << ": " << current_architecture << ": "
          << test_case.expected_result;
    }
  }
}

TEST(ManifestUtil, IsOSVersionCompatible) {
  EXPECT_TRUE(IsOSVersionCompatible({}));
  EXPECT_TRUE(IsOSVersionCompatible("6.0"));
  EXPECT_FALSE(IsOSVersionCompatible("60.0"));
  EXPECT_TRUE(IsOSVersionCompatible("0.1"));
  EXPECT_FALSE(IsOSVersionCompatible("foobar"));
}

TEST(ManifestUtil, IsOsSupported) {
  const std::string current_architecture = update_client::GetArchitecture();

  const struct {
    const char* platform;
    const char* arch_list;
    const char* min_os_version;
    const bool expected_result;
  } test_cases[] = {
      {"win", "x86", "6.0", true},
      {"mac", "x86", "6.0", false},
      {"win", "unknown", "6.0", false},
      {"win", "x64", "6.0", current_architecture == update_client::kArchAmd64},
      {"win", "-x64", "6.0", current_architecture != update_client::kArchAmd64},
      {"win", "x86,-x64", "6.0",
       current_architecture != update_client::kArchAmd64},
      {"win", "x86,x64,-arm64", "6.0",
       current_architecture != update_client::kArchArm64},
      {"win", "x86", "60.0", false},
      {"win", "x86", "0.01", true},
  };

  for (const auto& test_case : test_cases) {
    update_client::ProtocolParser::Results results;
    update_client::ProtocolParser::SystemRequirements& system_requirements =
        results.system_requirements;
    system_requirements.platform = test_case.platform;
    system_requirements.arch = test_case.arch_list;
    system_requirements.min_os_version = test_case.min_os_version;

    EXPECT_EQ(IsOsSupported(results), test_case.expected_result)
        << test_case.platform << ": " << test_case.arch_list << ": "
        << test_case.min_os_version << ": " << current_architecture << ": "
        << test_case.expected_result;
  }
}

}  // namespace updater
