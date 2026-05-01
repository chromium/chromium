// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/override_manifest_asset_manager_delegate.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

class OverrideManifestAssetManagerDelegateTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(OverrideManifestAssetManagerDelegateTest, ParsesConfigAndNotifies) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath config_path = temp_dir_.GetPath().AppendASCII("config.json");

  std::string json_content = R"({
    "manifest_path": "/test/manifest/dir",
    "components": {
      "key1": {
        "1.0.0.0": "/test/path/1"
      }
    }
  })";
  ASSERT_TRUE(base::WriteFile(config_path, json_content));

  OverrideManifestAssetManagerDelegate delegate(config_path);

  // Verify ListenForManifestReady
  base::test::TestFuture<base::FilePath> manifest_future;
  auto subscription =
      delegate.ListenForManifestReady(manifest_future.GetRepeatingCallback());
  EXPECT_EQ(manifest_future.Get(),
            base::FilePath(FILE_PATH_LITERAL("/test/manifest/dir")));

  // Verify GetFreeDiskSpace
  base::test::TestFuture<std::optional<base::ByteCount>> disk_space_future;
  delegate.GetFreeDiskSpace(disk_space_future.GetCallback());
  EXPECT_EQ(disk_space_future.Get(), base::GiB(100));
}

}  // namespace
}  // namespace optimization_guide
