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

TEST_F(OrganizerPanelViewTest, CallbackRunsWhenAnimationsDisabled) {
  CreateView();
  OrganizerPanelView::disable_animations_for_testing();

  // Show the panel (animations disabled -> instant show)
  state_controller()->SetOrganizerVisible(true);
  EXPECT_TRUE(state_controller()->IsOrganizerPanelVisible());
  organizer_panel_view()->OnOrganizerPanelStateChanged(state_controller());
  EXPECT_TRUE(organizer_panel_view()->GetVisible());

  organizer_panel_view()->set_on_close_animation_ended_callback_for_testing(
      panel_closed_callback_.Get());

  EXPECT_CALL(panel_closed_callback_, Run());

  // Hide the panel (animations disabled -> instant hide & callback run)
  state_controller()->SetOrganizerVisible(false);
  EXPECT_FALSE(state_controller()->IsOrganizerPanelVisible());
  organizer_panel_view()->OnOrganizerPanelStateChanged(state_controller());

  EXPECT_FALSE(organizer_panel_view()->GetVisible());
}

TEST_F(OrganizerPanelViewTest, CallbackDoesNotRunWhenVisible) {
  CreateView();
  OrganizerPanelView::disable_animations_for_testing();

  // Show the panel
  state_controller()->SetOrganizerVisible(true);
  EXPECT_TRUE(state_controller()->IsOrganizerPanelVisible());
  organizer_panel_view()->OnOrganizerPanelStateChanged(state_controller());

  organizer_panel_view()->set_on_close_animation_ended_callback_for_testing(
      panel_closed_callback_.Get());

  EXPECT_CALL(panel_closed_callback_, Run()).Times(0);

  // Re-trigger show (should not run callback)
  organizer_panel_view()->OnOrganizerPanelStateChanged(state_controller());

  EXPECT_TRUE(organizer_panel_view()->GetVisible());
}

class OrganizerPanelViewRTLTest : public OrganizerPanelViewTest {
 public:
  void SetUp() override {
    original_locale_ = base::i18n::GetConfiguredLocale();
    base::i18n::SetICUDefaultLocale("ar");
    OrganizerPanelViewTest::SetUp();
  }

  void TearDown() override {
    OrganizerPanelViewTest::TearDown();
    base::i18n::SetICUDefaultLocale(original_locale_);
  }

 private:
  std::string original_locale_;
};

TEST_F(OrganizerPanelViewRTLTest, RoundedCornersInRTL) {
  ASSERT_TRUE(base::i18n::IsRTL());
  CreateView();
  organizer_panel_view()->SetIsElevated(true);

  auto radii = organizer_panel_view()
                   ->content_container_for_testing()
                   ->layer()
                   ->rounded_corner_radii();

  // In RTL, we expect the left corners to be rounded.
  EXPECT_GT(radii.upper_left(), 0);
  EXPECT_GT(radii.lower_left(), 0);
  EXPECT_EQ(radii.upper_right(), 0);
  EXPECT_EQ(radii.lower_right(), 0);
}

TEST_F(OrganizerPanelViewRTLTest, ClipRectInRTL) {
  ASSERT_TRUE(base::i18n::IsRTL());
  CreateView();

  // Set some width to trigger layout.
  organizer_panel_view()->SetBounds(0, 0, 100, 600);
  organizer_panel_view()->SetTargetWidth(300);

  auto clip_rect = organizer_panel_view()->layer()->clip_rect();

  // In RTL, we expect the clip rect to extend to the left to allow the shadow.
  EXPECT_LT(clip_rect.x(), 0);
}

TEST_F(OrganizerPanelViewTest, CloseButtonFadeWhenExpandingOnMac) {
  CreateView();
  auto* controls_view = organizer_panel_view()->controls_view_for_testing();
  auto* organizer_button = controls_view->organizer_button_for_testing();
  gfx::SlideAnimation animation(organizer_panel_view());

#if BUILDFLAG(IS_MAC)
  // At 0.5 or less, opacity should be 0.
  animation.Reset(0.0);
  organizer_panel_view()->AnimationProgressed(&animation);
  EXPECT_FLOAT_EQ(0.0f, organizer_button->layer()->opacity());

  animation.Reset(0.5);
  organizer_panel_view()->AnimationProgressed(&animation);
  EXPECT_FLOAT_EQ(0.0f, organizer_button->layer()->opacity());

  // At 0.75, opacity should be 0.5.
  animation.Reset(0.75);
  organizer_panel_view()->AnimationProgressed(&animation);
  EXPECT_FLOAT_EQ(0.5f, organizer_button->layer()->opacity());

  // At 1.0, opacity should be 1.0.
  animation.Reset(1.0);
  organizer_panel_view()->AnimationProgressed(&animation);
  EXPECT_FLOAT_EQ(1.0f, organizer_button->layer()->opacity());
#else
  // On other platforms, it should always be 1.0.
  animation.Reset(0.0);
  organizer_panel_view()->AnimationProgressed(&animation);
  EXPECT_FLOAT_EQ(1.0f, organizer_button->layer()->opacity());

  animation.Reset(0.5);
  organizer_panel_view()->AnimationProgressed(&animation);
  EXPECT_FLOAT_EQ(1.0f, organizer_button->layer()->opacity());

  animation.Reset(1.0);
  organizer_panel_view()->AnimationProgressed(&animation);
  EXPECT_FLOAT_EQ(1.0f, organizer_button->layer()->opacity());
#endif
}

TEST_F(OrganizerPanelViewTest, WebViewExtendsToEdges) {
  CreateView();
  organizer_panel_view()->SetBounds(0, 0, 300, 600);
  organizer_panel_view()->SetTargetWidth(300);
  views::test::RunScheduledLayout(organizer_panel_view());

  auto* web_view = organizer_panel_view()->web_view_for_testing();
  ASSERT_TRUE(web_view);
  auto* container = organizer_panel_view()->content_container_for_testing();
  ASSERT_TRUE(container);

  // WebView should extend to the left (x = 0), right (width = container width),
  // and bottom (y + height = container height).
  EXPECT_EQ(web_view->bounds().x(), 0);
  EXPECT_EQ(web_view->bounds().width(), container->bounds().width());
  EXPECT_EQ(web_view->bounds().bottom(), container->bounds().height());
}

TEST_F(OrganizerPanelViewTest, NoWebViewWhenExtensionSidePanelFlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      organizer_panel::kShowExtensionsSidePanelUiInOrganizerPanel);

  CreateView();
  EXPECT_EQ(organizer_panel_view()->web_view_for_testing(), nullptr);
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
  EXPECT_NE(organizer_panel_view()->web_view_for_testing(), nullptr);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  EXPECT_FALSE(
      organizer_panel_view()->has_extension_observer_helper_for_testing());
#endif
}

TEST_F(OrganizerPanelViewTest, ExtensionStateControllerToggleLifecycle) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      organizer_panel::kShowExtensionsSidePanelUiInOrganizerPanel);

  CreateView();
  OrganizerPanelView::disable_animations_for_testing();

  EXPECT_FALSE(organizer_panel_view()->GetVisible());
  EXPECT_EQ(organizer_panel_view()->web_view_for_testing(), nullptr);

  // Toggle open
  state_controller()->SetOrganizerVisible(true);
  organizer_panel_view()->OnOrganizerPanelStateChanged(state_controller());
  EXPECT_TRUE(organizer_panel_view()->GetVisible());

  // Toggle close
  state_controller()->SetOrganizerVisible(false);
  organizer_panel_view()->OnOrganizerPanelStateChanged(state_controller());
  EXPECT_FALSE(organizer_panel_view()->GetVisible());
  EXPECT_EQ(organizer_panel_view()->web_view_for_testing(), nullptr);
}
