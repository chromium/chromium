// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_container.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_organization_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "fake_base_tab_strip_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

class FakeBaseTabStripControllerWithProfile
    : public FakeBaseTabStripController {
 public:
  Profile* GetProfile() const override { return profile_.get(); }

 private:
  std::unique_ptr<TestingProfile> profile_ = std::make_unique<TestingProfile>();
};

class TabSearchContainerTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    scoped_feature_list_.InitWithFeatures(
        {features::kTabOrganization, features::kChromeRefresh2023}, {});

    auto controller = std::make_unique<FakeBaseTabStripControllerWithProfile>();
    tab_strip_ = std::make_unique<TabStrip>(std::move(controller));
    container_before_tab_strip_ =
        std::make_unique<TabSearchContainer>(tab_strip_.get(), true);
    container_after_tab_strip_ =
        std::make_unique<TabSearchContainer>(tab_strip_.get(), false);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TabStrip> tab_strip_;
  std::unique_ptr<TabSearchContainer> container_before_tab_strip_;
  std::unique_ptr<TabSearchContainer> container_after_tab_strip_;
};

TEST_F(TabSearchContainerTest, OrdersButtonsCorrectly) {
  ASSERT_EQ(container_before_tab_strip_->tab_search_button(),
            container_before_tab_strip_->children()[0]);
  ASSERT_EQ(container_before_tab_strip_->tab_organization_button(),
            container_before_tab_strip_->children()[1]);

  ASSERT_EQ(container_after_tab_strip_->tab_organization_button(),
            container_after_tab_strip_->children()[0]);
  ASSERT_EQ(container_after_tab_strip_->tab_search_button(),
            container_after_tab_strip_->children()[1]);
}

TEST_F(TabSearchContainerTest, ButtonsHaveFlatEdges) {
  ASSERT_EQ(Edge::kRight,
            container_before_tab_strip_->tab_search_button()->flat_edge());
  ASSERT_EQ(
      Edge::kLeft,
      container_before_tab_strip_->tab_organization_button()->flat_edge());

  ASSERT_EQ(Edge::kLeft,
            container_after_tab_strip_->tab_search_button()->flat_edge());
  ASSERT_EQ(Edge::kRight,
            container_after_tab_strip_->tab_organization_button()->flat_edge());
}

TEST_F(TabSearchContainerTest, AnimatesToExpanded) {
  // Should be collapsed by default
  ASSERT_EQ(0, container_before_tab_strip_->expansion_animation_for_testing()
                   ->GetCurrentValue());

  ASSERT_EQ(0, container_before_tab_strip_->tab_organization_button()
                   ->width_factor_for_testing());
  ASSERT_EQ(1, container_before_tab_strip_->tab_organization_button()
                   ->flat_edge_factor_for_testing());
  ASSERT_EQ(1, container_before_tab_strip_->tab_search_button()
                   ->flat_edge_factor_for_testing());

  container_before_tab_strip_->ShowTabOrganization();

  ASSERT_TRUE(container_before_tab_strip_->expansion_animation_for_testing()
                  ->IsShowing());

  container_before_tab_strip_->expansion_animation_for_testing()->Reset(1);

  ASSERT_EQ(1, container_before_tab_strip_->tab_organization_button()
                   ->width_factor_for_testing());
  ASSERT_EQ(0, container_before_tab_strip_->tab_organization_button()
                   ->flat_edge_factor_for_testing());
  ASSERT_EQ(0, container_before_tab_strip_->tab_search_button()
                   ->flat_edge_factor_for_testing());
}

TEST_F(TabSearchContainerTest, TogglesActionUIState) {
  ASSERT_FALSE(container_before_tab_strip_->expansion_animation_for_testing()
                   ->IsShowing());
  ASSERT_EQ(nullptr, container_before_tab_strip_->tab_organization_button()
                         ->session_for_testing());

  TabOrganizationService* service =
      container_before_tab_strip_->tab_organization_service_for_testing();
  service->OnTriggerOccured(nullptr);

  ASSERT_TRUE(container_before_tab_strip_->expansion_animation_for_testing()
                  ->IsShowing());
  ASSERT_NE(nullptr, container_before_tab_strip_->tab_organization_button()
                         ->session_for_testing());
}
