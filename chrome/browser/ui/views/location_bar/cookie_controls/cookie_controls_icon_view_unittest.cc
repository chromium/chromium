// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"

#include <string_view>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "cookie_controls_bubble_coordinator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/views/accessibility/ax_update_notifier.h"
#include "ui/views/test/ax_event_counter.h"

namespace {

std::u16string AllowedLabel() {
  return l10n_util::GetStringUTF16(
      IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_ALLOWED_LABEL);
}

std::u16string SiteNotWorkingLabel() {
  return l10n_util::GetStringUTF16(
      IDS_TRACKING_PROTECTION_PAGE_ACTION_SITE_NOT_WORKING_LABEL);
}

const char kUMAIconShown[] = "TrackingProtection.UserBypass.Shown";
const char kUMAIconOpened[] = "TrackingProtection.UserBypass.Shown.Opened";
const char kUMAIconAnimated[] = "TrackingProtection.UserBypass.Animated";
const char kUMAIconAnimatedOpened[] =
    "TrackingProtection.UserBypass.Animated.Opened";
const char kUMABubbleOpenedBlocked[] =
    "CookieControls.Bubble.CookiesBlocked.Opened";
const char kUMABubbleOpenedAllowed[] =
    "CookieControls.Bubble.CookiesAllowed.Opened";

// A fake CookieControlsBubbleCoordinator that has a no-op ShowBubble().
class FakeCookieControlsBubbleCoordinator
    : public CookieControlsBubbleCoordinator {
 public:
  explicit FakeCookieControlsBubbleCoordinator(
      BrowserWindowInterface* browser_window)
      : CookieControlsBubbleCoordinator(
            browser_window,
            browser_window->GetActions()->root_action_item()) {}

  void ShowBubble(
      ToolbarButtonProvider* provider,
      content::WebContents* web_contents,
      content_settings::CookieControlsController* controller) override {}

  CookieControlsBubbleViewImpl* GetBubble() const override { return nullptr; }
};

}  // namespace

class CookieControlsIconViewUnitTest
    : public TestWithBrowserView,
      public testing::WithParamInterface<CookieBlocking3pcdStatus> {
 protected:
  CookieControlsIconViewUnitTest()
      : a11y_counter_(views::AXUpdateNotifier::Get()) {}

  void SetUp() override {
    // This test should be rewritten as a raw unit test instead of using
    // TestWithBrowserView. All of the upstream dependencies of the icon view
    // should be mocked.
    coordinator_override_ =
        BrowserWindowFeatures::GetUserDataFactoryForTesting()
            .AddOverrideForTesting(
                base::BindRepeating([](BrowserWindowInterface& browser) {
                  return std::make_unique<FakeCookieControlsBubbleCoordinator>(
                      &browser);
                }));

    TestWithBrowserView::SetUp();

    delegate_ = browser_view()->GetLocationBarView();

    auto icon_view = std::make_unique<CookieControlsIconView>(
        browser(), delegate_, delegate_);
    icon_view->SetCoordinatorForTesting(
        *CookieControlsBubbleCoordinator::From(browser()));
    view_ = browser_view()->GetLocationBarView()->AddChildView(
        std::move(icon_view));

    AddTab(browser(), GURL("chrome://newtab"));
  }

  void TearDown() override {
    delegate_ = nullptr;
    view_ = nullptr;
    TestWithBrowserView::TearDown();
  }

  bool In3pcd() { return GetParam() != CookieBlocking3pcdStatus::kNotIn3pcd; }

  std::u16string BlockedLabel() {
    return GetParam() == CookieBlocking3pcdStatus::kLimited
               ? l10n_util::GetStringUTF16(
                     IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_LIMITED_LABEL)
               : l10n_util::GetStringUTF16(
                     IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_BLOCKED_LABEL);
  }

  bool LabelShown() { return view_->ShouldShowLabel(); }

  bool Visible() { return view_->ShouldBeVisible(); }

  std::u16string_view LabelText() const { return view_->label()->GetText(); }

  std::u16string_view TooltipText() const {
    return view_->IconLabelBubbleView::GetTooltipText();
  }

  void ExecuteIcon() {
    view_->OnExecuting(
        CookieControlsIconView::ExecuteSource::EXECUTE_SOURCE_MOUSE);
  }

  // Wait for any pending events in the message queue to be processed.
  void FlushEvents() {
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  base::UserActionTester user_actions_;
  views::test::AXEventCounter a11y_counter_;
  raw_ptr<CookieControlsIconView> view_;

 private:
  base::test::ScopedFeatureList feature_list_;
  ui::UserDataFactory::ScopedOverride coordinator_override_;
  raw_ptr<LocationBarView> delegate_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CookieControlsIconViewUnitTest,
                         testing::Values(CookieBlocking3pcdStatus::kNotIn3pcd,
                                         CookieBlocking3pcdStatus::kLimited,
                                         CookieBlocking3pcdStatus::kAll));

/// Enabled third-party cookie blocking.

TEST_P(CookieControlsIconViewUnitTest, DefaultNotVisible) {
  EXPECT_FALSE(Visible());
  EXPECT_FALSE(LabelShown());
}

TEST_P(CookieControlsIconViewUnitTest,
       IconAnimatesWhenShouldHighlightIsTrueAnd3pcsBlocked) {
  view_->OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
  FlushEvents();
  EXPECT_TRUE(Visible());
  EXPECT_TRUE(LabelShown());
  EXPECT_EQ(TooltipText(), BlockedLabel());
  EXPECT_EQ(LabelText(), In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());
// TODO(b/436858103): Fix screenreader tests on ChromeOS and Mac.
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 1);
#endif
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimated), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimatedOpened), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconShown), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconOpened), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedBlocked), 1);
}

TEST_P(CookieControlsIconViewUnitTest,
       IconAnimatesOnPageReloadWithChanged3pcSettings) {
  view_->OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/false);
  FlushEvents();
  ExecuteIcon();
  // Force the icon to animate and set the label again
  view_->OnFinishedPageReloadWithChangedSettings();

  EXPECT_TRUE(Visible());
  EXPECT_TRUE(LabelShown());
  EXPECT_EQ(TooltipText(), BlockedLabel());
  EXPECT_EQ(LabelText(), BlockedLabel());
}

TEST_P(CookieControlsIconViewUnitTest,
       IconAnimationTextDoesNotResetWhenStateDoesNotChange) {
  view_->OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
  FlushEvents();
  EXPECT_TRUE(Visible());
  EXPECT_TRUE(LabelShown());
  EXPECT_EQ(LabelText(), In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());

  view_->OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
  FlushEvents();
  EXPECT_EQ(LabelText(), In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());
}

TEST_P(CookieControlsIconViewUnitTest,
       IconAnimationTextUpdatesWhen3pcStateChanges) {
  view_->OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
  FlushEvents();
  EXPECT_TRUE(Visible());
  EXPECT_TRUE(LabelShown());
  EXPECT_EQ(LabelText(), In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());

  view_->OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kAllowed3pc, GetParam(),
      /*should_highlight=*/true);
  FlushEvents();
  EXPECT_EQ(LabelText(), AllowedLabel());
}

TEST_P(CookieControlsIconViewUnitTest, IconAnimationIsResetOnWebContentChange) {
  view_->OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
  FlushEvents();
  EXPECT_TRUE(Visible());
  EXPECT_TRUE(LabelShown());
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimated), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimatedOpened), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconShown), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconOpened), 0);
  // Simulate a change in web content.
  view_->UpdateImpl();
  view_->OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/false);
  FlushEvents();
  ExecuteIcon();
  EXPECT_TRUE(Visible());
  EXPECT_FALSE(LabelShown());
  // Animated user actions should not be counted.
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimated), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimatedOpened), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconShown), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconOpened), 1);
}

TEST_P(CookieControlsIconViewUnitTest, HidingIconDoesNotRetriggerA11yReadOut) {
  view_->OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/true);
  FlushEvents();
  EXPECT_TRUE(Visible());
  EXPECT_TRUE(LabelShown());
  EXPECT_EQ(TooltipText(), BlockedLabel());
  EXPECT_EQ(LabelText(), In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());
// TODO(b/436858103): Fix screenreader tests on ChromeOS and Mac.
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 1);
#endif

  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimated), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconShown), 0);
  view_->OnCookieControlsIconStatusChanged(
      /*icon_visible=*/false, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/false);
  FlushEvents();
  EXPECT_FALSE(Visible());
  EXPECT_FALSE(LabelShown());
  EXPECT_EQ(TooltipText(), BlockedLabel());
  EXPECT_EQ(LabelText(), In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());
  // We don't read out the label again when the icon becomes hidden.
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 1);
#endif
  // Verify no metrics are recorded when icon is hidden.
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimated), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconShown), 0);
}

TEST_P(CookieControlsIconViewUnitTest,
       IconDoesNotAnimateWhenShouldHighlightIsFalse) {
  view_->OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/false);
  FlushEvents();
  EXPECT_TRUE(Visible());
  EXPECT_FALSE(LabelShown());
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimated), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimatedOpened), 0);
}

TEST_P(CookieControlsIconViewUnitTest, IconHiddenWhenIconVisibleIsFalse) {
  view_->OnCookieControlsIconStatusChanged(
      /*icon_visible=*/false, CookieControlsState::kAllowed3pc, GetParam(),
      /*should_highlight=*/false);
  FlushEvents();
  EXPECT_FALSE(Visible());
  EXPECT_FALSE(LabelShown());
  EXPECT_EQ(TooltipText(), u"");
  EXPECT_EQ(LabelText(), u"");
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 0);
#endif
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconShown), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimated), 0);
}

TEST_P(CookieControlsIconViewUnitTest, RecordsIconOpenMetricWhen3pcsAllowed) {
  view_->OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kAllowed3pc, GetParam(),
      /*should_highlight=*/false);
  FlushEvents();
  EXPECT_TRUE(Visible());
  EXPECT_EQ(TooltipText(), AllowedLabel());
  EXPECT_EQ(LabelText(), AllowedLabel());
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconShown), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconOpened), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedAllowed), 1);
}

TEST_P(CookieControlsIconViewUnitTest, RecordsIconOpenMetricWhen3pcsBlocked) {
  view_->OnCookieControlsIconStatusChanged(
      /*icon_visible=*/true, CookieControlsState::kBlocked3pc, GetParam(),
      /*should_highlight=*/false);
  FlushEvents();
  EXPECT_EQ(TooltipText(), BlockedLabel());
  EXPECT_EQ(LabelText(), BlockedLabel());
  EXPECT_TRUE(Visible());
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconShown), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconOpened), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedBlocked), 1);
}
