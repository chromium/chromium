// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_container.h"

#include <memory>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/fake_tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "fake_base_tab_strip_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/gfx/animation/animation_test_api.h"

using ::testing::NiceMock;

class FakeBaseTabStripControllerWithBWI : public FakeBaseTabStripController {
 public:
  explicit FakeBaseTabStripControllerWithBWI(
      BrowserWindowInterface* browser_window_interface)
      : browser_window_interface_(browser_window_interface) {}

  BrowserWindowInterface* GetBrowserWindowInterface() override {
    return browser_window_interface_;
  }

 private:
  raw_ptr<BrowserWindowInterface> browser_window_interface_;
};

class TabSearchContainerTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    TabOrganizationUtils::GetInstance()->SetIgnoreOptGuideForTesting(true);
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kTabOrganization},
        /*disabled_features=*/{features::kTabstripDeclutter});

    tab_strip_model_ = std::make_unique<TabStripModel>(
        &tab_strip_model_delegate_, profile_.get());

    browser_window_interface_ = std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*browser_window_interface_, GetTabStripModel())
        .WillByDefault(::testing::Return(tab_strip_model_.get()));
    ON_CALL(*browser_window_interface_, GetProfile())
        .WillByDefault(::testing::Return(profile_.get()));
    ON_CALL(*browser_window_interface_, GetFeatures())
        .WillByDefault(testing::ReturnRef(browser_window_features_));
    ON_CALL(testing::Const(*browser_window_interface_), GetFeatures())
        .WillByDefault(testing::ReturnRef(browser_window_features_));
    ON_CALL(*browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(user_data_host_));

    tab_strip_ = std::make_unique<TabStrip>(
        std::make_unique<FakeBaseTabStripControllerWithBWI>(
            browser_window_interface_.get()),
        std::unique_ptr<NiceMock<TabHoverCardController>>());

    tab_declutter_controller_ = std::make_unique<tabs::TabDeclutterController>(
        browser_window_interface_.get());

    locked_expansion_view_ = std::make_unique<views::View>();
    container_before_tab_strip_ = std::make_unique<TabSearchContainer>(
        true, locked_expansion_view_.get(), tab_strip_.get());
    container_after_tab_strip_ = std::make_unique<TabSearchContainer>(
        false, locked_expansion_view_.get(), tab_strip_.get());
  }

 protected:
  void ResetAnimation(int value) {
    if (container_before_tab_strip_->animation_session_for_testing()) {
      container_before_tab_strip_->animation_session_for_testing()
          ->ResetOpacityAnimationForTesting(value);
    }
    if (container_before_tab_strip_->animation_session_for_testing()) {
      container_before_tab_strip_->animation_session_for_testing()
          ->ResetExpansionAnimationForTesting(value);
    }
    if (container_before_tab_strip_->animation_session_for_testing()) {
      container_before_tab_strip_->animation_session_for_testing()
          ->ResetFlatEdgeAnimationForTesting(value);
    }
  }

  std::unique_ptr<TestingProfile> profile_ = std::make_unique<TestingProfile>();
  base::test::ScopedFeatureList scoped_feature_list_;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<tabs::TabDeclutterController> tab_declutter_controller_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  std::unique_ptr<TabStrip> tab_strip_;
  std::unique_ptr<views::View> locked_expansion_view_;
  std::unique_ptr<TabSearchContainer> container_before_tab_strip_;
  std::unique_ptr<TabSearchContainer> container_after_tab_strip_;

  BrowserWindowFeatures browser_window_features_;

  // Some of these tests rely on animation being enabled. This forces
  // animation on even if it's turned off in the OS.
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_{
      gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)};
};

TEST_F(TabSearchContainerTest, OrdersButtonsCorrectly) {
  ASSERT_EQ(container_before_tab_strip_->tab_search_button(),
            container_before_tab_strip_->children()[0]);
  ASSERT_EQ(container_before_tab_strip_->auto_tab_group_button(),
            container_before_tab_strip_->children()[1]);

  ASSERT_EQ(container_after_tab_strip_->auto_tab_group_button(),
            container_after_tab_strip_->children()[0]);
  ASSERT_EQ(container_after_tab_strip_->tab_search_button(),
            container_after_tab_strip_->children()[1]);
}

TEST_F(TabSearchContainerTest, ButtonsHaveFlatEdges) {
  ASSERT_EQ(
      Edge::kRight,
      container_before_tab_strip_->tab_search_button()->animated_flat_edge());
  ASSERT_EQ(Edge::kLeft, container_before_tab_strip_->auto_tab_group_button()
                             ->animated_flat_edge());

  ASSERT_EQ(
      Edge::kLeft,
      container_after_tab_strip_->tab_search_button()->animated_flat_edge());
  ASSERT_EQ(Edge::kRight, container_after_tab_strip_->auto_tab_group_button()
                              ->animated_flat_edge());
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

  ResetAnimation(1);

  ASSERT_EQ(1, container_before_tab_strip_->auto_tab_group_button()
                   ->width_factor_for_testing());
}
