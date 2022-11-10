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

}  // namespace updater
