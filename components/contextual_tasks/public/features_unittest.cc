// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/features.h"

#include "base/test/scoped_feature_list.h"
#include "components/contextual_tasks/public/host_override.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

TEST(FeaturesTest, ForcedEmbeddedPageHost_NoOverride) {
  EXPECT_EQ(std::nullopt, GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest, ForcedEmbeddedPageHost_OverrideToGoogleHost) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      kContextualTasks,
      {{"contextual-tasks-forced-embedded-page-host", "corp.google.com"}});

  EXPECT_EQ((HostOverride{"corp.google.com"}), GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest, ForcedEmbeddedPageHost_OverrideToNonGoogleHost) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      kContextualTasks,
      {{"contextual-tasks-forced-embedded-page-host", "example.com"}});

  EXPECT_EQ(std::nullopt, GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest, ForcedEmbeddedPageHost_OverrideToNonGoogleHost_BadSuffix) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      kContextualTasks,
      {{"contextual-tasks-forced-embedded-page-host", "corpgoogle.com"}});

  EXPECT_EQ(std::nullopt, GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest, ForcedEmbeddedPageHost_OverrideToNonGoogleHost_Subdomain) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      kContextualTasks, {{"contextual-tasks-forced-embedded-page-host",
                          "google.com.example.com"}});

  EXPECT_EQ(std::nullopt, GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest, ForcedEmbeddedPageHost_SetOverrideRuntime) {
  SetForcedEmbeddedPageHostOverride(HostOverride{"localhost.corp.google.com"});
  EXPECT_EQ((HostOverride{"localhost.corp.google.com"}),
            GetForcedEmbeddedPageHost());

  SetForcedEmbeddedPageHostOverride(std::nullopt);
  EXPECT_EQ(std::nullopt, GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest, IsContextualTasksUnboundedMenuEnabled_DefaultDisabled) {
  EXPECT_FALSE(IsContextualTasksUnboundedMenuEnabled());
}

TEST(FeaturesTest, IsContextualTasksUnboundedMenuEnabled_FlagEnabled) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(kContextualTasksUnboundedMenu);
  EXPECT_TRUE(IsContextualTasksUnboundedMenuEnabled());
}

TEST(FeaturesTest,
     IsContextualTasksUnboundedMenuEnabled_SidePanelRearchitectureEnabled) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(kContextualTasksSidePanelRearchitecture);
  EXPECT_TRUE(IsContextualTasksUnboundedMenuEnabled());
}

}  // namespace contextual_tasks
