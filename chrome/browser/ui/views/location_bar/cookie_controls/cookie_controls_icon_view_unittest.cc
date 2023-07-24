// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_icon_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/features.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
std::u16string AllowedLabel() {
  return l10n_util::GetStringUTF16(
      IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_ALLOWED_LABEL);
}

std::u16string BlockedLabel() {
  return l10n_util::GetStringUTF16(
      IDS_COOKIE_CONTROLS_PAGE_ACTION_COOKIES_BLOCKED_LABEL);
}
}  // namespace

class CookieControlsIconViewUnitTest : public TestWithBrowserView {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        content_settings::features::kUserBypassUI);
    TestWithBrowserView::SetUp();

    delegate_ = browser_view()->GetLocationBarView();
    view_ = std::make_unique<CookieControlsIconView>(delegate_, delegate_);
    AddTab(browser(), GURL("chrome://newtab"));
  }

  void TearDown() override {
    delegate_ = nullptr;
    view_.reset();
    TestWithBrowserView::TearDown();
  }

  bool LabelShown() { return view_->ShouldShowLabel(); }

  bool Visible() { return view_->ShouldBeVisible(); }

  const std::u16string& LabelText() { return view_->label()->GetText(); }

  std::u16string TooltipText() {
    return view_->IconLabelBubbleView::GetTooltipText();
  }

  std::unique_ptr<CookieControlsIconView> view_;

 private:
  base::test::ScopedFeatureList feature_list_;

  raw_ptr<LocationBarView> delegate_;
};

/// Enabled third-party cookie blocking.

TEST_F(CookieControlsIconViewUnitTest, DefaultNotVisible) {
  EXPECT_FALSE(Visible());
  EXPECT_FALSE(LabelShown());
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
}
