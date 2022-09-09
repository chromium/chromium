// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/manifest_util.h"

#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(ManifestUtil, ReadInstallCommandFromManifest) {
  base::FilePath installer_path;
  std::string install_args;
  std::string install_data;

  base::FilePath offline_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &offline_dir));
  offline_dir = offline_dir.Append(FILE_PATH_LITERAL("updater"));

  ReadInstallCommandFromManifest(
      offline_dir, "{CDABE316-39CD-43BA-8440-6D1E0547AEE6}", "verboselogging",
      installer_path, install_args, install_data);
  EXPECT_EQ(installer_path, offline_dir.AppendASCII("my_installer.exe"));
  EXPECT_EQ(install_args, "-baz");
  EXPECT_EQ(install_data,
            "{\n"
            "        \"distribution\": {\n"
            "          \"verbose_logging\": true\n"
            "        }\n"
            "      }");
}

}  // namespace updater
