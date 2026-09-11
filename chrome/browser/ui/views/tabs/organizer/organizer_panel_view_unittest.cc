// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"

class OrganizerPanelViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    EXPECT_CALL(mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));

    profile_ = std::make_unique<TestingProfile>();
    EXPECT_CALL(mock_browser_window_interface_, GetProfile())
        .WillRepeatedly(testing::Return(profile()));

    // Create a root action item for the panel.
    root_action_item_ =
        actions::ActionItem::Builder()
            .AddChildren(actions::ActionItem::Builder().SetActionId(
                kActionToggleOrganizerPanel))
            .Build();
    browser_actions_ =
        std::make_unique<BrowserActions>(&mock_browser_window_interface_);
    browser_actions_->set_root_action_item_for_testing(root_action_item_.get());
    animation_controller_ = std::make_unique<BrowserAnimationController>(
        mock_browser_window_interface_);
    animation_controller_->AddAnimationProvider(
        std::make_unique<OrganizerPanelAnimations>());
    state_controller_ = std::make_unique<OrganizerPanelController>(
        mock_browser_window_interface_, root_action_item_.get());
  }

  void CreateView() {
    auto view = OrganizerPanelView::Create(mock_browser_window_interface_);
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    view_ = widget_->SetContentsView(std::move(view));
    widget_->SetBounds(gfx::Rect(0, 0, 800, 600));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    // Widget owns the view, so it will delete it.
    // We need to close widget first.
    if (widget_ && !widget_->IsClosed()) {
      widget_->CloseNow();
    }
    widget_.reset();

    state_controller_.reset();
    animation_controller_.reset();
    if (browser_actions_) {
      browser_actions_->set_root_action_item_for_testing(nullptr);
      browser_actions_.reset();
    }
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestingProfile* profile() { return profile_.get(); }

 protected:
  OrganizerPanelController* state_controller() {
    return state_controller_.get();
  }

  OrganizerPanelView* organizer_panel_view() { return view_; }

  views::View* GetWebView() {
    auto* const tracker = views::ElementTrackerViews::GetInstance();
    return tracker->GetFirstMatchingView(
        OrganizerPanelView::kWebViewElementId,
        tracker->GetContextForWidget(widget_.get()));
  }

  base::MockCallback<base::OnceClosure> panel_closed_callback_;

 private:
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<actions::ActionItem> root_action_item_;
  std::unique_ptr<BrowserActions> browser_actions_;
  std::unique_ptr<BrowserAnimationController> animation_controller_;
  std::unique_ptr<OrganizerPanelController> state_controller_;

  // Widget owns the view.
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<OrganizerPanelView> view_ = nullptr;
};

TEST_F(OrganizerPanelViewTest, NoWebViewWhenExtensionSidePanelFlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      organizer_panel::kShowExtensionsSidePanelUiInOrganizerPanel);

  CreateView();
  EXPECT_EQ(GetWebView(), nullptr);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_TRUE(organizer_panel_view()->IsInExtensionModeForTesting());
#else
  EXPECT_FALSE(organizer_panel_view()->IsInExtensionModeForTesting());
#endif
}

TEST_F(OrganizerPanelViewTest, DefaultWebViewCreatedWhenFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      organizer_panel::kShowExtensionsSidePanelUiInOrganizerPanel);

  CreateView();
  EXPECT_NE(GetWebView(), nullptr);
  EXPECT_FALSE(organizer_panel_view()->IsInExtensionModeForTesting());
}
