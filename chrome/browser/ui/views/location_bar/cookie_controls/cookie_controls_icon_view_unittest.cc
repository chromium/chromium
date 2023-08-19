// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"

#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/features.h"
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

const char kUMAHighConfidenceShown[] = "CookieControls.HighConfidence.Shown";
const char kUMAHighConfidenceOpened[] = "CookieControls.HighConfidence.Opened";
const char kUMAMediumConfidenceShown[] =
    "CookieControls.MediumConfidence.Shown";
const char kUMAMediumConfidenceOpened[] =
    "CookieControls.MediumConfidence.Opened";
const char kUMABubbleOpenedBlocked[] =
    "CookieControls.Bubble.CookiesBlocked.Opened";
const char kUMABubbleOpenedAllowed[] =
    "CookieControls.Bubble.CookiesAllowed.Opened";
const char kUMABubbleOpenedUnknown[] =
    "CookieControls.Bubble.UnknownState.Opened";

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

class CookieControlsIconViewUnitTest : public TestWithBrowserView {
 protected:
  CookieControlsIconViewUnitTest()
      : a11y_counter_(views::AXEventManager::Get()) {}
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        content_settings::features::kUserBypassUI);
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

  base::UserActionTester user_actions_;
  views::test::AXEventCounter a11y_counter_;
  raw_ptr<CookieControlsIconView> view_;

 private:
  base::test::ScopedFeatureList feature_list_;

  raw_ptr<LocationBarView> delegate_;
};

/// Enabled third-party cookie blocking.

TEST_F(CookieControlsIconViewUnitTest, DefaultNotVisible) {
  EXPECT_FALSE(Visible());
  EXPECT_FALSE(LabelShown());
  // Execute a improperly initialized icon view.
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedUnknown), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedBlocked), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedAllowed), 0);
}

TEST_F(CookieControlsIconViewUnitTest, HighConfidenceEnabled) {
  view_->OnStatusChanged(CookieControlsStatus::kEnabled,
                         CookieControlsEnforcement::kEnforcedByCookieSetting,
                         base::Time::Now() + base::Days(10));
  view_->OnBreakageConfidenceLevelChanged(
      CookieControlsBreakageConfidenceLevel::kHigh);
  EXPECT_TRUE(Visible());
  EXPECT_TRUE(LabelShown());  // Animation for high confidence
  EXPECT_EQ(TooltipText(), BlockedLabel());
  EXPECT_EQ(LabelText(), BlockedLabel());
// TODO(crbug.com/1446230): Fix screenreader tests on ChromeOS and Mac.
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 1);
#endif
  EXPECT_EQ(user_actions_.GetActionCount(kUMAHighConfidenceShown), 1);
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMAHighConfidenceOpened), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedBlocked), 1);
}

TEST_F(CookieControlsIconViewUnitTest, MediumConfidenceEnabled) {
  view_->OnStatusChanged(CookieControlsStatus::kEnabled,
                         CookieControlsEnforcement::kEnforcedByCookieSetting,
                         base::Time::Now() + base::Days(10));
  view_->OnBreakageConfidenceLevelChanged(
      CookieControlsBreakageConfidenceLevel::kMedium);
  EXPECT_TRUE(Visible());
  EXPECT_FALSE(LabelShown());
  EXPECT_EQ(TooltipText(), BlockedLabel());
  EXPECT_EQ(LabelText(), BlockedLabel());
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 0);
#endif
  EXPECT_EQ(user_actions_.GetActionCount(kUMAMediumConfidenceShown), 1);
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMAMediumConfidenceOpened), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedBlocked), 1);
}

TEST_F(CookieControlsIconViewUnitTest, LowConfidenceEnabled) {
  view_->OnStatusChanged(CookieControlsStatus::kEnabled,
                         CookieControlsEnforcement::kEnforcedByCookieSetting,
                         base::Time::Now() + base::Days(10));
  view_->OnBreakageConfidenceLevelChanged(
      CookieControlsBreakageConfidenceLevel::kLow);
  EXPECT_FALSE(Visible());
  EXPECT_FALSE(LabelShown());
  EXPECT_EQ(TooltipText(), BlockedLabel());
  EXPECT_EQ(LabelText(), BlockedLabel());
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 0);
#endif
  EXPECT_EQ(user_actions_.GetActionCount(kUMAHighConfidenceShown), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAMediumConfidenceShown), 0);
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedBlocked), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedAllowed), 0);
}

//// Default third-party cookie blocking disabled.

TEST_F(CookieControlsIconViewUnitTest, HighConfidenceDisabled) {
  view_->OnStatusChanged(CookieControlsStatus::kDisabled,
                         CookieControlsEnforcement::kEnforcedByCookieSetting,
                         base::Time::Now() + base::Days(10));
  view_->OnBreakageConfidenceLevelChanged(
      CookieControlsBreakageConfidenceLevel::kHigh);
  EXPECT_FALSE(Visible());
  EXPECT_FALSE(LabelShown());
  EXPECT_EQ(TooltipText(), AllowedLabel());
  EXPECT_EQ(LabelText(), AllowedLabel());
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 0);
#endif
  EXPECT_EQ(user_actions_.GetActionCount(kUMAHighConfidenceShown), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAMediumConfidenceShown), 0);
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedAllowed), 1);
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedBlocked), 0);
}

TEST_F(CookieControlsIconViewUnitTest, MediumConfidenceDisabled) {
  view_->OnStatusChanged(CookieControlsStatus::kDisabled,
                         CookieControlsEnforcement::kEnforcedByCookieSetting,
                         base::Time::Now() + base::Days(10));
  view_->OnBreakageConfidenceLevelChanged(
      CookieControlsBreakageConfidenceLevel::kMedium);
  EXPECT_FALSE(Visible());
  EXPECT_FALSE(LabelShown());
  EXPECT_EQ(TooltipText(), AllowedLabel());
  EXPECT_EQ(LabelText(), AllowedLabel());
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 0);
#endif
  EXPECT_EQ(user_actions_.GetActionCount(kUMAHighConfidenceShown), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAMediumConfidenceShown), 0);
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedAllowed), 1);
}

TEST_F(CookieControlsIconViewUnitTest, LowConfidenceDisabled) {
  view_->OnStatusChanged(CookieControlsStatus::kDisabled,
                         CookieControlsEnforcement::kEnforcedByCookieSetting,
                         base::Time::Now() + base::Days(10));
  view_->OnBreakageConfidenceLevelChanged(
      CookieControlsBreakageConfidenceLevel::kLow);
  EXPECT_FALSE(Visible());
  EXPECT_FALSE(LabelShown());
  EXPECT_EQ(TooltipText(), AllowedLabel());
  EXPECT_EQ(LabelText(), AllowedLabel());
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 0);
#endif
  EXPECT_EQ(user_actions_.GetActionCount(kUMAHighConfidenceShown), 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAMediumConfidenceShown), 0);
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedAllowed), 1);
}

/// Disabled third-party cookie blocking for site.

TEST_F(CookieControlsIconViewUnitTest, HighConfidenceDisabledForSite) {
  view_->OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                         CookieControlsEnforcement::kEnforcedByCookieSetting,
                         base::Time::Now() + base::Days(10));
  view_->OnBreakageConfidenceLevelChanged(
      CookieControlsBreakageConfidenceLevel::kHigh);
  EXPECT_TRUE(Visible());
  EXPECT_TRUE(LabelShown());
  EXPECT_EQ(TooltipText(), AllowedLabel());
  EXPECT_EQ(LabelText(), AllowedLabel());
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 1);
#endif
  EXPECT_EQ(user_actions_.GetActionCount(kUMAHighConfidenceShown), 1);
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedAllowed), 1);
}

TEST_F(CookieControlsIconViewUnitTest, MediumConfidenceDisabledForSite) {
  view_->OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                         CookieControlsEnforcement::kEnforcedByCookieSetting,
                         base::Time::Now() + base::Days(10));
  view_->OnBreakageConfidenceLevelChanged(
      CookieControlsBreakageConfidenceLevel::kMedium);
  EXPECT_TRUE(Visible());
  EXPECT_FALSE(LabelShown());
  EXPECT_EQ(TooltipText(), AllowedLabel());
  EXPECT_EQ(LabelText(), AllowedLabel());
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 0);
#endif
  EXPECT_EQ(user_actions_.GetActionCount(kUMAMediumConfidenceShown), 1);
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedAllowed), 1);
}

TEST_F(CookieControlsIconViewUnitTest, LowConfidenceDisabledForSite) {
  view_->OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                         CookieControlsEnforcement::kEnforcedByCookieSetting,
                         base::Time::Now() + base::Days(10));
  view_->OnBreakageConfidenceLevelChanged(
      CookieControlsBreakageConfidenceLevel::kLow);
  EXPECT_FALSE(Visible());
  EXPECT_FALSE(LabelShown());
  EXPECT_EQ(TooltipText(), AllowedLabel());
  EXPECT_EQ(LabelText(), AllowedLabel());
#if !OS_MAC && !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(a11y_counter_.GetCount(ax::mojom::Event::kAlert), 0);
#endif
  ExecuteIcon();
  EXPECT_EQ(user_actions_.GetActionCount(kUMABubbleOpenedAllowed), 1);
}
