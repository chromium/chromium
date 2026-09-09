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
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_state_controller.h"
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
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"

class OrganizerPanelViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    profile_ = std::make_unique<TestingProfile>();

    // Create a root action item for the panel.
    root_action_item_ =
        actions::ActionItem::Builder()
            .AddChildren(actions::ActionItem::Builder().SetActionId(
                kActionToggleOrganizerPanel))
            .Build();

    // Create a real State Controller.
    EXPECT_CALL(mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));

    state_controller_ = std::make_unique<OrganizerPanelStateController>(
        &mock_browser_window_interface_, root_action_item_.get());

    EXPECT_CALL(mock_browser_window_interface_, GetProfile())
        .WillRepeatedly(testing::Return(profile()));
  }

  void CreateView() {
    auto view = std::make_unique<OrganizerPanelView>(
        &mock_browser_window_interface_, root_action_item_.get(),
        state_controller_.get());
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
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestingProfile* profile() { return profile_.get(); }

 protected:
  OrganizerPanelStateController* state_controller() {
    return state_controller_.get();
  }

  OrganizerPanelView* organizer_panel_view() { return view_; }

  base::MockCallback<base::OnceClosure> panel_closed_callback_;

 private:
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<actions::ActionItem> root_action_item_;
  std::unique_ptr<OrganizerPanelStateController> state_controller_;

  // Widget owns the view.
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<OrganizerPanelView> view_ = nullptr;
};

TEST_F(OrganizerPanelViewTest, NoWebViewWhenExtensionSidePanelFlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      organizer_panel::kShowExtensionsSidePanelUiInOrganizerPanel);

  CreateView();
  EXPECT_EQ(organizer_panel_view()->GetWebViewForTesting(), nullptr);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_TRUE(
      organizer_panel_view()->has_extension_observer_helper_for_testing());
#endif
}

TEST_F(OrganizerPanelViewTest, DefaultWebViewCreatedWhenFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      organizer_panel::kShowExtensionsSidePanelUiInOrganizerPanel);

  CreateView();
  EXPECT_NE(organizer_panel_view()->GetWebViewForTesting(), nullptr);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_FALSE(
      organizer_panel_view()->has_extension_observer_helper_for_testing());
#endif
}
