// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_container.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_organization_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
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

    TabOrganizationUtils::GetInstance()->SetIgnoreOptGuideForTesting(true);
    scoped_feature_list_.InitWithFeatures(
        {features::kTabOrganization, features::kTabstripDeclutter}, {});

    tab_strip_controller_ =
        std::make_unique<FakeBaseTabStripControllerWithProfile>();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        &tab_strip_model_delegate_, tab_strip_controller_->GetProfile());

    browser_window_interface_ = std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*browser_window_interface_, GetTabStripModel)
        .WillByDefault(::testing::Return(tab_strip_model_.get()));

    tab_declutter_controller_ = std::make_unique<tabs::TabDeclutterController>(
        browser_window_interface_.get());

    locked_expansion_view_ = std::make_unique<views::View>();
    container_before_tab_strip_ = std::make_unique<TabSearchContainer>(
        tab_strip_controller_.get(), tab_strip_model_.get(), true,
        locked_expansion_view_.get(), tab_declutter_controller_.get());
    container_after_tab_strip_ = std::make_unique<TabSearchContainer>(
        tab_strip_controller_.get(), tab_strip_model_.get(), false,
        locked_expansion_view_.get(), tab_declutter_controller_.get());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TabStripController> tab_strip_controller_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<tabs::TabDeclutterController> tab_declutter_controller_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  std::unique_ptr<views::View> locked_expansion_view_;
  std::unique_ptr<TabSearchContainer> container_before_tab_strip_;
  std::unique_ptr<TabSearchContainer> container_after_tab_strip_;
};

TEST_F(TabSearchContainerTest, OrdersButtonsCorrectly) {
  ASSERT_EQ(container_before_tab_strip_->tab_search_button(),
            container_before_tab_strip_->children()[0]);
  ASSERT_EQ(container_before_tab_strip_->tab_declutter_button(),
            container_before_tab_strip_->children()[1]);
  ASSERT_EQ(container_before_tab_strip_->auto_tab_group_button(),
            container_before_tab_strip_->children()[2]);

  ASSERT_EQ(container_after_tab_strip_->tab_declutter_button(),
            container_after_tab_strip_->children()[0]);
  ASSERT_EQ(container_after_tab_strip_->auto_tab_group_button(),
            container_after_tab_strip_->children()[1]);
  ASSERT_EQ(container_after_tab_strip_->tab_search_button(),
            container_after_tab_strip_->children()[2]);
}

TEST_F(TabSearchContainerTest, ButtonsHaveFlatEdges) {
  ASSERT_EQ(Edge::kRight,
            container_before_tab_strip_->tab_search_button()->flat_edge());
  ASSERT_EQ(Edge::kLeft,
            container_before_tab_strip_->auto_tab_group_button()->flat_edge());

  ASSERT_EQ(Edge::kLeft,
            container_after_tab_strip_->tab_search_button()->flat_edge());
  ASSERT_EQ(Edge::kRight,
            container_after_tab_strip_->auto_tab_group_button()->flat_edge());
}

TEST_F(TabSearchContainerTest, AnimatesToExpanded) {
  // Should be collapsed by default
  ASSERT_EQ(nullptr,
            container_before_tab_strip_->animation_session_for_testing());

  ASSERT_EQ(0, container_before_tab_strip_->auto_tab_group_button()
                   ->width_factor_for_testing());

  container_before_tab_strip_->ShowTabOrganization(
      container_before_tab_strip_->auto_tab_group_button());

  ASSERT_TRUE(container_before_tab_strip_->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());

  container_before_tab_strip_->animation_session_for_testing()
      ->ResetAnimationForTesting(1);

  ASSERT_EQ(1, container_before_tab_strip_->auto_tab_group_button()
                   ->width_factor_for_testing());
}
