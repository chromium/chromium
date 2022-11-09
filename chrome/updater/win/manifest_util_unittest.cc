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

TEST(ManifestUtil, IsArchCompatible_ArchEmpty) {
  EXPECT_TRUE(IsArchCompatible({}));
}

TEST(ManifestUtil, IsArchCompatible_ArchUnknown) {
  EXPECT_FALSE(IsArchCompatible("unknown"));
}

TEST(ManifestUtil, IsArchCompatible_Archx86) {
  EXPECT_TRUE(IsArchCompatible("x86"));
}

TEST(ManifestUtil, IsArchCompatible_Archx64) {
  EXPECT_EQ(update_client::GetArchitecture() == update_client::kArchAmd64,
            IsArchCompatible("x64"));
}

TEST(ManifestUtil, IsArchCompatible_NotArchx64) {
  // TODO(crbug.com/1382666) : re-examine `IsArchCompatible` impl.
  if (update_client::GetArchitecture() == update_client::kArchAmd64)
    EXPECT_FALSE(IsArchCompatible("-x64"));
}

TEST(ManifestUtil, IsArchCompatible_Archx86Notx64) {
  // TODO(crbug.com/1382666) : re-examine `IsArchCompatible` impl.
  if (update_client::GetArchitecture() == update_client::kArchAmd64)
    EXPECT_FALSE(IsArchCompatible("x86,-x64"));
}

TEST(ManifestUtil, IsArchCompatible_Archx86x64NotArm64) {
  // TODO(crbug.com/1382666) : re-examine `IsArchCompatible` impl.
  if (update_client::GetArchitecture() == update_client::kArchArm64)
    EXPECT_FALSE(IsArchCompatible("x86,x64,-arm64"));
}

}  // namespace updater
