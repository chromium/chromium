// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/manifest_util.h"

#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(ManifestUtil, ReadInstallCommandFromManifest) {
  update_client::ProtocolParser::Results results;
  base::FilePath installer_path;
  std::string install_args;
  std::string install_data;

  base::FilePath offline_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &offline_dir));
  offline_dir = offline_dir.Append(FILE_PATH_LITERAL("updater"));

  ReadInstallCommandFromManifest(
      offline_dir, "{CDABE316-39CD-43BA-8440-6D1E0547AEE6}", "verboselogging",
      results, installer_path, install_args, install_data);
  EXPECT_EQ(installer_path, offline_dir.AppendASCII("my_installer.exe"));
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
