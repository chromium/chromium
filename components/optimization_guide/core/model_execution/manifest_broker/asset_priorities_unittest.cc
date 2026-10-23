// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/asset_priorities.h"

#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_execution/configs/manifest_builder.h"
#include "components/optimization_guide/core/model_execution/on_device_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

BaseModelRecipeArgs GenericRecipeArgs() {
  return BaseModelRecipeArgs(
      proto::BaseModelRecipe::BACKEND_TYPE_CPU,
      proto::BaseModelRecipe::PERFORMANCE_HINT_FASTEST_INFERENCE, {1}, 1024);
}

TEST(AssetPrioritiesTest, RaiseAndIsAtLeast) {
  AssetPriorities priorities;

  priorities.Raise(AssetPriority::kUninstall, {"uninstall_asset"});
  priorities.Raise(AssetPriority::kEvictable, {"evictable_asset"});
  priorities.Raise(AssetPriority::kRetain, {"retain_asset"});
  priorities.Raise(AssetPriority::kSpeculative, {"speculative_asset"});
  priorities.Raise(AssetPriority::kBestEffort, {"best_effort_asset"});
  priorities.Raise(AssetPriority::kUserBlocking, {"user_blocking_asset"});

  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kUninstall, "uninstall_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kEvictable, "uninstall_asset"));

  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kEvictable, "evictable_asset"));
  EXPECT_FALSE(priorities.IsAtLeast(AssetPriority::kRetain, "evictable_asset"));

  EXPECT_TRUE(priorities.IsAtLeast(AssetPriority::kEvictable, "retain_asset"));
  EXPECT_TRUE(priorities.IsAtLeast(AssetPriority::kRetain, "retain_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kSpeculative, "retain_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kBestEffort, "retain_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kUserBlocking, "retain_asset"));

  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kEvictable, "speculative_asset"));
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kRetain, "speculative_asset"));
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kSpeculative, "speculative_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kBestEffort, "speculative_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kUserBlocking, "speculative_asset"));

  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kEvictable, "best_effort_asset"));
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kRetain, "best_effort_asset"));
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kSpeculative, "best_effort_asset"));
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kBestEffort, "best_effort_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kUserBlocking, "best_effort_asset"));

  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kEvictable, "user_blocking_asset"));
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kRetain, "user_blocking_asset"));
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kSpeculative, "user_blocking_asset"));
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kBestEffort, "user_blocking_asset"));
  EXPECT_TRUE(priorities.IsAtLeast(AssetPriority::kUserBlocking,
                                   "user_blocking_asset"));

  // Unknown assets default to kEvictable.
  EXPECT_TRUE(priorities.IsAtLeast(AssetPriority::kEvictable, "unknown_asset"));
  EXPECT_FALSE(priorities.IsAtLeast(AssetPriority::kRetain, "unknown_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kSpeculative, "unknown_asset"));

  // Raising to a lower priority does not decrease existing priority.
  priorities.Raise(AssetPriority::kSpeculative, {"user_blocking_asset"});
  EXPECT_TRUE(priorities.IsAtLeast(AssetPriority::kUserBlocking,
                                   "user_blocking_asset"));

  // Raising from kUninstall to a higher priority increases priority.
  priorities.Raise(AssetPriority::kEvictable, {"uninstall_asset"});
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kEvictable, "uninstall_asset"));

  // Raising to a higher priority increases priority.
  priorities.Raise(AssetPriority::kUserBlocking, {"best_effort_asset"});
  EXPECT_TRUE(
      priorities.IsAtLeast(AssetPriority::kUserBlocking, "best_effort_asset"));

  priorities.Clear();
  EXPECT_TRUE(priorities.IsAtLeast(AssetPriority::kEvictable, "retain_asset"));
  EXPECT_FALSE(priorities.IsAtLeast(AssetPriority::kRetain, "retain_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kSpeculative, "speculative_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kSpeculative, "best_effort_asset"));
  EXPECT_FALSE(
      priorities.IsAtLeast(AssetPriority::kSpeculative, "user_blocking_asset"));
}

TEST(AssetPrioritiesTest, ComputeFromManifestAndUsageTracker) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestingPrefServiceSimple local_state;
  model_execution::prefs::RegisterLocalStatePrefs(local_state.registry());
  UsageTracker usage_tracker(&local_state);

  auto manifest = Manifest::Create(
      base::FilePath(),
      ManifestBuilder()
          .Add("asset_a", OnDemandComponent("key_a", "1.0"))
          .Add("asset_b", OnDemandComponent("key_b", "1.0"))
          .Add("shared_asset", OnDemandComponent("key_shared", "1.0"))
          .Add("base_a", BaseModelRecipe(FileReference("asset_a", "a.bin"),
                                         GenericRecipeArgs()))
          .Add("base_b", BaseModelRecipe(FileReference("asset_b", "b.bin"),
                                         GenericRecipeArgs()))
          .Add({DeviceCategory::kCpu, "used_feature"},
               SolutionRecipe("base_a",
                              FileReference("shared_asset", "config_a.pb")))
          .Add({DeviceCategory::kCpu, "unused_feature"},
               SolutionRecipe("base_b",
                              FileReference("shared_asset", "config_b.pb")))
          .Build(),
      DeviceCategory::kCpu);
  ASSERT_TRUE(manifest.has_value());

  // Record usage for "used_feature", while "unused_feature" is never used.
  usage_tracker.RaisePriority("used_feature",
                              UsageTracker::Priority::kUserBlocking);
  {
    AssetPriorities priorities(*manifest, usage_tracker);
    EXPECT_TRUE(priorities.IsAtLeast(AssetPriority::kUserBlocking, "asset_a"));
    EXPECT_TRUE(
        priorities.IsAtLeast(AssetPriority::kUserBlocking, "shared_asset"));
    // "asset_b" is only required by never-used "unused_feature", so it defaults
    // to kEvictable.
    EXPECT_TRUE(priorities.IsAtLeast(AssetPriority::kEvictable, "asset_b"));
    EXPECT_FALSE(priorities.IsAtLeast(AssetPriority::kRetain, "asset_b"));
  }

  // Requesting uninstall for "used_feature" lowers both "asset_a" and
  // "shared_asset" to kUninstall, without never-used "unused_feature" raising
  // "shared_asset" back to kEvictable.
  usage_tracker.RequestUninstall("used_feature");
  {
    AssetPriorities priorities(*manifest, usage_tracker);
    EXPECT_TRUE(priorities.IsAtLeast(AssetPriority::kUninstall, "asset_a"));
    EXPECT_FALSE(priorities.IsAtLeast(AssetPriority::kEvictable, "asset_a"));
    EXPECT_TRUE(
        priorities.IsAtLeast(AssetPriority::kUninstall, "shared_asset"));
    EXPECT_FALSE(
        priorities.IsAtLeast(AssetPriority::kEvictable, "shared_asset"));
  }
}

}  // namespace
}  // namespace optimization_guide
