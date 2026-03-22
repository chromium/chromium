// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_utils.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

struct ProjectsPanelVisibilityParams {
  // Whether the Projects Panel feature is enabled.
  bool feature_enabled;
  // Whether tab groups are enabled.
  bool tab_groups_enabled_for_profile;
  // Expected visibility of the panel.
  bool expected_visibility;
};

class ProjectsPanelUtilsTestBase : public testing::Test {
 public:
  ProjectsPanelUtilsTestBase() {
    TestingProfile::Builder profile_with_tab_groups_builder;
    profile_with_tab_groups_builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<tab_groups::MockTabGroupSyncService>();
        }));
    profile_with_tab_groups_enabled_ = profile_with_tab_groups_builder.Build();

    TestingProfile::Builder profile_without_tab_groups_builder_;
    profile_without_tab_groups_builder_.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> { return nullptr; }));
    profile_with_tab_groups_disabled_ =
        profile_without_tab_groups_builder_.Build();
  }

  Profile* GetProfileWithTabGroupsEnabled() {
    return profile_with_tab_groups_enabled_.get();
  }

  Profile* GetProfileWithTabGroupsDisabled() {
    return profile_with_tab_groups_disabled_.get();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<TestingProfile> profile_with_tab_groups_enabled_;
  std::unique_ptr<TestingProfile> profile_with_tab_groups_disabled_;
};

class ProjectsPanelUtilsTest
    : public ProjectsPanelUtilsTestBase,
      public testing::WithParamInterface<ProjectsPanelVisibilityParams> {
 public:
  void SetUp() override {
    if (GetParam().feature_enabled) {
      scoped_feature_list_.InitAndEnableFeature(tab_groups::kProjectsPanel);
    } else {
      scoped_feature_list_.InitAndDisableFeature(tab_groups::kProjectsPanel);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ProjectsPanelUtilsTest, ReturnsCorrectVisibility) {
  Profile* profile = GetParam().tab_groups_enabled_for_profile
                         ? GetProfileWithTabGroupsEnabled()
                         : GetProfileWithTabGroupsDisabled();

  EXPECT_EQ(projects_panel::IsProjectsPanelVisibleForProfile(profile),
            GetParam().expected_visibility);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProjectsPanelUtilsTest,
    testing::Values(
        // Panel should be visible.
        ProjectsPanelVisibilityParams{/*featue_enabled=*/true,
                                      /*tab_groups_enabled_for_profile=*/true,
                                      /*expected_visibility=*/true},
        ProjectsPanelVisibilityParams{/*featue_enabled=*/true,
                                      /*tab_groups_enabled_for_profile=*/false,
                                      /*expected_visibility=*/false},
        // Panel should not be visible.
        ProjectsPanelVisibilityParams{/*featue_enabled=*/false,
                                      /*tab_groups_enabled_for_profile=*/true,
                                      /*expected_visibility=*/false},
        ProjectsPanelVisibilityParams{/*featue_enabled=*/false,
                                      /*tab_groups_enabled_for_profile=*/false,
                                      /*expected_visibility=*/false}));
