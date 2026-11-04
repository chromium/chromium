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

  EXPECT_EQ((HostOverride{"corp.google.com", std::nullopt}),
            GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest, ForcedEmbeddedPageHost_OverrideToGoogleHostWithPort) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      kContextualTasks, {{"contextual-tasks-forced-embedded-page-host",
                          "localhost.corp.google.com:8888"}});

  EXPECT_EQ((HostOverride{"localhost.corp.google.com", 8888}),
            GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest, ForcedEmbeddedPageHost_OverrideToGoogleHost_WithHttps) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      kContextualTasks, {{"contextual-tasks-forced-embedded-page-host",
                          "https://localhost.corp.google.com"}});

  EXPECT_EQ((HostOverride{"localhost.corp.google.com", std::nullopt}),
            GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest,
     ForcedEmbeddedPageHost_OverrideToGoogleHost_WithHttpsAndPort) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      kContextualTasks, {{"contextual-tasks-forced-embedded-page-host",
                          "https://localhost.corp.google.com:8888"}});

  EXPECT_EQ((HostOverride{"localhost.corp.google.com", 8888}),
            GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest, ForcedEmbeddedPageHost_OverrideToNonGoogleHost) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      kContextualTasks,
      {{"contextual-tasks-forced-embedded-page-host", "example.com"}});

  EXPECT_EQ(std::nullopt, GetForcedEmbeddedPageHost());
}

TEST(FeaturesTest, ForcedEmbeddedPageHost_OverrideToNonGoogleHostWithPort) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      kContextualTasks,
      {{"contextual-tasks-forced-embedded-page-host", "example.com:8888"}});

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
  SetForcedEmbeddedPageHostOverride(
      HostOverride{"localhost.corp.google.com", 9090});
  EXPECT_EQ((HostOverride{"localhost.corp.google.com", 9090}),
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

TEST(FeaturesTest, IsContextualTasksUIEnabled_SidePanelRearchitectureEnabled) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures({kContextualTasksSidePanelRearchitecture},
                                   {kContextualTasks, kContextualTasksSidePanel,
                                    kContextualTasksRearchitecture});
  EXPECT_TRUE(IsContextualTasksUIEnabled());
}

TEST(FeaturesTest,
     IsContextualTasksEphemeralEntryPointEnabled_DefaultDisabled) {
  EXPECT_FALSE(IsContextualTasksEphemeralEntryPointEnabled());
}

TEST(FeaturesTest, IsContextualTasksEphemeralEntryPointEnabled_LegacyParam) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeatureWithParameters(
      kContextualTasksEphemeralBrandedEntryPoint,
      {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}});
  EXPECT_TRUE(IsContextualTasksEphemeralEntryPointEnabled());
}

TEST(FeaturesTest,
     IsContextualTasksEphemeralEntryPointEnabled_PinButtonRefactor) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures(
      {kContextualTasksPinningV2, kContextualTasksEphemeralPinButtonRefactor},
      {});
  EXPECT_TRUE(IsContextualTasksEphemeralEntryPointEnabled());
}

TEST(FeaturesTest,
     IsContextualTasksEphemeralEntryPointEnabled_RefactorRequiresPinningV2) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeatures({kContextualTasksEphemeralPinButtonRefactor},
                                   {kContextualTasksPinningV2});
  EXPECT_FALSE(IsContextualTasksEphemeralEntryPointEnabled());
}

TEST(FeaturesTest, GetEphemeralButtonContextMenuOption_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndDisableFeature(
      kContextualTasksEphemeralButtonContextMenu);
  EXPECT_EQ(EphemeralButtonContextMenuOption::kHideSidePanel,
            GetEphemeralButtonContextMenuOption());
}

TEST(FeaturesTest, GetEphemeralButtonContextMenuOption_EnumOptions) {
  struct TestCase {
    const char* param_value;
    EphemeralButtonContextMenuOption expected_option;
  };
  constexpr TestCase kTestCases[] = {
      {"hide-side-panel", EphemeralButtonContextMenuOption::kHideSidePanel},
      {"close-side-panel", EphemeralButtonContextMenuOption::kCloseSidePanel},
      {"dismiss-active-search",
       EphemeralButtonContextMenuOption::kDismissActiveSearch},
      {"dismiss-active-search-in-tab",
       EphemeralButtonContextMenuOption::kDismissActiveSearchInTab},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.param_value);
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitAndEnableFeatureWithParameters(
        kContextualTasksEphemeralButtonContextMenu,
        {{"ContextualTasksEphemeralButtonContextMenuOption",
          test_case.param_value}});
    EXPECT_EQ(test_case.expected_option, GetEphemeralButtonContextMenuOption());
  }
}

TEST(FeaturesTest, GetEphemeralButtonContextMenuOption_LegacyBooleanFallback) {
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitAndEnableFeatureWithParameters(
        kContextualTasksEphemeralButtonContextMenu,
        {{"ContextualTasksEphemeralButtonContextMenuCloseSidePanel", "true"}});
    EXPECT_EQ(EphemeralButtonContextMenuOption::kCloseSidePanel,
              GetEphemeralButtonContextMenuOption());
  }
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitAndEnableFeatureWithParameters(
        kContextualTasksEphemeralButtonContextMenu,
        {{"ContextualTasksEphemeralButtonContextMenuCloseSidePanel", "false"}});
    EXPECT_EQ(EphemeralButtonContextMenuOption::kHideSidePanel,
              GetEphemeralButtonContextMenuOption());
  }
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitAndEnableFeatureWithParameters(
        kContextualTasksEphemeralButtonContextMenu,
        {{"ContextualTasksEphemeralButtonContextMenuOption",
          "dismiss-active-search"},
         {"ContextualTasksEphemeralButtonContextMenuCloseSidePanel", "true"}});
    EXPECT_EQ(EphemeralButtonContextMenuOption::kDismissActiveSearch,
              GetEphemeralButtonContextMenuOption());
  }
}

}  // namespace contextual_tasks
