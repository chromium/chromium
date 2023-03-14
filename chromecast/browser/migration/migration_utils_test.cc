// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/migration/migration_utils.h"

#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_path_override.h"
#include "chromecast/base/cast_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace cast_browser_migration {

namespace {

constexpr char kCastBrowserConfigName[] = ".cast_browser.conf";
constexpr char kEurekaConfigName[] = ".eureka.conf";
constexpr char kLargeConfigExtension[] = ".large";
constexpr char kTestConfigString[] = "test config";
constexpr char kTestLargeConfigString[] = "test large config string";

TEST(MigrationUtilsTest, CopySucceed) {
  // Initialize.
  base::ScopedTempDir tmp_dir;
  EXPECT_TRUE(tmp_dir.CreateUniqueTempDir());
  base::FilePath old_config_path = tmp_dir.GetPath().Append(kEurekaConfigName);
  base::FilePath new_config_path =
      tmp_dir.GetPath().Append(kCastBrowserConfigName);
  base::FilePath new_large_config_path =
      new_config_path.AddExtension(kLargeConfigExtension);

  base::WriteFile(old_config_path, kTestConfigString);
  base::WriteFile(old_config_path.AddExtension(kLargeConfigExtension),
                  kTestLargeConfigString);

  base::ScopedPathOverride eureka_config_override(
      FILE_CAST_CONFIG, old_config_path, /*is_absolute=*/true,
      /*create=*/false);
  base::ScopedPathOverride cast_browser_config_override(
      FILE_CAST_BROWSER_CONFIG, new_config_path, /*is_absolute=*/true,
      /*create=*/false);

  // Execute.
  EXPECT_TRUE(CopyPrefConfigsIfMissing());

  // Verify.
  EXPECT_TRUE(base::PathExists(new_config_path));
  std::string new_config_data;
  EXPECT_TRUE(base::ReadFileToString(new_config_path, &new_config_data));
  EXPECT_EQ(new_config_data, kTestConfigString);

  EXPECT_TRUE(base::PathExists(new_large_config_path));
  std::string new_large_config_data;
  EXPECT_TRUE(
      base::ReadFileToString(new_large_config_path, &new_large_config_data));
  EXPECT_EQ(new_large_config_data, kTestLargeConfigString);
}

}  // namespace

}  // namespace cast_browser_migration
}  // namespace chromecast
