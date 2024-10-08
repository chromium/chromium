// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "cookie_controls_bubble_coordinator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/test/ax_event_counter.h"

namespace {
using ::testing::NiceMock;

std::u16string AllowedLabel() {
  return l10n_util::GetStringUTF16(
      IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_ALLOWED_LABEL);
}

std::u16string BlockedLabel() {
  return l10n_util::GetStringUTF16(
      IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_BLOCKED_LABEL);
}

std::u16string LimitedLabel() {
  return l10n_util::GetStringUTF16(
      IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_LIMITED_LABEL);
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
class MockCookieControlsBubbleCoordinator
    : public CookieControlsBubbleCoordinator {
 public:
  MOCK_METHOD(void,
              ShowBubble,
              (content::WebContents * web_contents,
               content_settings::CookieControlsController* controller),
              (override));
  MOCK_METHOD(CookieControlsBubbleViewImpl*, GetBubble, (), (const, override));
};

}  // namespace

class CookieControlsIconViewUnitTest
    : public TestWithBrowserView,
      public testing::WithParamInterface<CookieBlocking3pcdStatus> {
 protected:
  CookieControlsIconViewUnitTest()
      : a11y_counter_(views::AXEventManager::Get()) {
    feature_list_.InitAndDisableFeature(
        privacy_sandbox::kTrackingProtection3pcdUx);
  }

  void SetUp() override {
    TestWithBrowserView::SetUp();

    delegate_ = browser_view()->GetLocationBarView();

    auto icon_view = std::make_unique<CookieControlsIconView>(
        browser(), delegate_, delegate_);
    auto fake_coordinator =
        std::make_unique<NiceMock<MockCookieControlsBubbleCoordinator>>();
    icon_view->SetCoordinatorForTesting(std::move(fake_coordinator));
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

  std::u16string TrackingProtectionLabel() {
    if (GetParam() == CookieBlocking3pcdStatus::kLimited) {
      return LimitedLabel();
    }
    return BlockedLabel();
  }

  bool LabelShown() { return view_->ShouldShowLabel(); }

  bool Visible() { return view_->ShouldBeVisible(); }

  const std::u16string& LabelText() { return view_->label()->GetText(); }

  std::u16string TooltipText() {
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
       IconAnimatesWhenShouldHighlightIsTrueAndProtectionsAreOn) {
  view_->OnCookieControlsIconStatusChanged(/*icon_visible=*/true,
                                           /*protections_on=*/true, GetParam(),
                                           /*should_highlight=*/true);
  FlushEvents();
  EXPECT_TRUE(Visible());
  EXPECT_TRUE(LabelShown());
  EXPECT_EQ(TooltipText(),
            In3pcd() ? TrackingProtectionLabel() : BlockedLabel());
  EXPECT_EQ(LabelText(), In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());
// TODO(crbug.com/40064612): Fix screenreader tests on ChromeOS and Mac.
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS_ASH)
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
       IconAnimatesOnPageReloadWithChangedSettings) {
  view_->OnCookieControlsIconStatusChanged(/*icon_visible=*/true,
                                           /*protections_on=*/true, GetParam(),
                                           /*should_highlight=*/false);
  FlushEvents();
  ExecuteIcon();
  // Force the icon to animate and set the label again
  view_->OnFinishedPageReloadWithChangedSettings();

  EXPECT_TRUE(Visible());
  EXPECT_TRUE(LabelShown());
  EXPECT_EQ(TooltipText(),
            In3pcd() ? TrackingProtectionLabel() : BlockedLabel());
  EXPECT_EQ(LabelText(), GetParam() == CookieBlocking3pcdStatus::kLimited
                             ? LimitedLabel()
                             : BlockedLabel());
}

TEST_P(CookieControlsIconViewUnitTest,
       IconAnimationTextDoesNotResetWhenProtectionsDoNotChange) {
  view_->OnCookieControlsIconStatusChanged(/*icon_visible=*/true,
                                           /*protections_on=*/true, GetParam(),
                                           /*should_highlight=*/true);
  FlushEvents();
  EXPECT_TRUE(Visible());
  EXPECT_TRUE(LabelShown());
  EXPECT_EQ(LabelText(), In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());

  view_->OnCookieControlsIconStatusChanged(/*icon_visible=*/true,
                                           /*protections_on=*/true, GetParam(),
                                           /*should_highlight=*/true);
  FlushEvents();
  EXPECT_EQ(LabelText(), In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());
}

TEST_P(CookieControlsIconViewUnitTest,
       IconAnimationTextUpdatesWhenProtectionsChange) {
  view_->OnCookieControlsIconStatusChanged(/*icon_visible=*/true,
                                           /*protections_on=*/true, GetParam(),
                                           /*should_highlight=*/true);
  FlushEvents();
  EXPECT_TRUE(Visible());
  EXPECT_TRUE(LabelShown());
  EXPECT_EQ(LabelText(), In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());

  view_->OnCookieControlsIconStatusChanged(/*icon_visible=*/true,
                                           /*protections_on=*/false, GetParam(),
                                           /*should_highlight=*/true);
  FlushEvents();
  EXPECT_EQ(LabelText(), AllowedLabel());
}

TEST_P(CookieControlsIconViewUnitTest, IconAnimationIsResetOnWebContentChange) {
  view_->OnCookieControlsIconStatusChanged(/*icon_visible=*/true,
                                           /*protections_on=*/true, GetParam(),
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
  view_->OnCookieControlsIconStatusChanged(/*icon_visible=*/true,
                                           /*protections_on=*/true, GetParam(),
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
  view_->OnCookieControlsIconStatusChanged(/*icon_visible=*/true,
                                           /*protections_on=*/true, GetParam(),
                                           /*should_highlight=*/true);
  FlushEvents();
  EXPECT_TRUE(Visible());
  EXPECT_TRUE(LabelShown());
  EXPECT_EQ(TooltipText(),
            In3pcd() ? TrackingProtectionLabel() : BlockedLabel());
  EXPECT_EQ(LabelText(), In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());
// TODO(crbug.com/40064612): Fix screenreader tests on ChromeOS and Mac.
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 1);
#endif

  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimated), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconShown), 0);
  view_->OnCookieControlsIconStatusChanged(/*icon_visible=*/false,
                                           /*protections_on=*/true, GetParam(),
                                           /*should_highlight=*/false);
  FlushEvents();
  EXPECT_FALSE(Visible());
  EXPECT_FALSE(LabelShown());
  EXPECT_EQ(TooltipText(),
            In3pcd() ? TrackingProtectionLabel() : BlockedLabel());
  EXPECT_EQ(LabelText(), In3pcd() ? SiteNotWorkingLabel() : BlockedLabel());
  // We don't read out the label again when the icon becomes hidden.
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 1);
#endif
  // Verify no metrics are recorded when icon is hidden.
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimated), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconShown), 0);
}

TEST_P(CookieControlsIconViewUnitTest,
       IconDoesNotAnimateWhenShouldHighlightIsFalse) {
  view_->OnCookieControlsIconStatusChanged(/*icon_visible=*/true,
                                           /*protections_on=*/true, GetParam(),
                                           /*should_highlight=*/false);
  FlushEvents();
  EXPECT_TRUE(Visible());
  EXPECT_FALSE(LabelShown());
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimated), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimatedOpened), 0);
}

TEST_P(CookieControlsIconViewUnitTest, IconHiddenWhenIconVisibleIsFalse) {
  view_->OnCookieControlsIconStatusChanged(/*icon_visible=*/false,
                                           /*protections_on=*/false, GetParam(),
                                           /*should_highlight=*/false);
  FlushEvents();
  EXPECT_FALSE(Visible());
  EXPECT_FALSE(LabelShown());
  EXPECT_EQ(TooltipText(), u"");
  EXPECT_EQ(LabelText(), u"");
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 0);
#endif
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconShown), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconAnimated), 0);
}

TEST_P(CookieControlsIconViewUnitTest,
       RecordsIconOpenMetricWhenProtectionsAreOff) {
  view_->OnCookieControlsIconStatusChanged(/*icon_visible=*/true,
                                           /*protections_on=*/false, GetParam(),
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

TEST_P(CookieControlsIconViewUnitTest,
       RecordsIconOpenMetricWhenProtectionsAreOn) {
  view_->OnCookieControlsIconStatusChanged(/*icon_visible=*/true,
                                           /*protections_on=*/true, GetParam(),
                                           /*should_highlight=*/false);
  FlushEvents();
  EXPECT_EQ(TooltipText(),
            In3pcd() ? TrackingProtectionLabel() : BlockedLabel());
  EXPECT_EQ(LabelText(), In3pcd() ? TrackingProtectionLabel() : BlockedLabel());
  EXPECT_TRUE(Visible());
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconShown), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIconOpened), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedBlocked), 1);
}
