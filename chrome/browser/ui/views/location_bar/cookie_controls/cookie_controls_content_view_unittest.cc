// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_content_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/tracking_protection_feature.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

using ::testing::Contains;
using ::testing::Eq;
using ::testing::Property;
using ::ui::ImageModel;

class CookieControlsContentViewUnitTest : public TestWithBrowserView {
 public:
  CookieControlsContentViewUnitTest()
      : view_(std::make_unique<CookieControlsContentView>(
            /*has_act_features=*/false)) {}

 protected:
  views::View* GetFeedbackButton() {
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
        CookieControlsContentView::kFeedbackButton,
        views::ElementTrackerViews::GetContextForView(
            view_->feedback_section_));
  }

  views::ToggleButton* GetToggleButton() { return view_->toggle_button_; }
  CookieControlsContentView* GetContentView() { return view_.get(); }

  std::unique_ptr<CookieControlsContentView> view_;
};

namespace {

TEST_F(CookieControlsContentViewUnitTest, FeedbackSection) {
  EXPECT_THAT(
      GetFeedbackButton()->GetViewAccessibility().GetCachedName(),
      Eq(base::JoinString(
          {l10n_util::GetStringUTF16(
               IDS_COOKIE_CONTROLS_BUBBLE_SEND_FEEDBACK_BUTTON_TITLE),
           l10n_util::GetStringUTF16(
               IDS_COOKIE_CONTROLS_BUBBLE_SEND_FEEDBACK_BUTTON_DESCRIPTION)},
          u" \n")));
}

TEST_F(CookieControlsContentViewUnitTest, ToggleButton_Initial) {
  EXPECT_THAT(GetToggleButton()->GetViewAccessibility().GetCachedName(),
              Eq(l10n_util::GetStringUTF16(
                  IDS_COOKIE_CONTROLS_BUBBLE_THIRD_PARTY_COOKIES_LABEL)));
}

TEST_F(CookieControlsContentViewUnitTest, ToggleButton_UpdatedSites) {
  const std::u16string label = u"17 sites allowed";
  GetContentView()->SetCookiesLabel(label);
  std::u16string expected = base::JoinString(
      {l10n_util::GetStringUTF16(
           IDS_COOKIE_CONTROLS_BUBBLE_THIRD_PARTY_COOKIES_LABEL),
       label},
      u"\n");
  // TODO: convert to AllOf(HasSubstr(), HasSubStr()) when gtest supports
  // u16string.
  EXPECT_THAT(GetToggleButton()->GetViewAccessibility().GetCachedName(),
              Eq(expected));
}

}  // namespace

class CookieControlsContentViewTrackingProtectionUnitTest
    : public TestWithBrowserView {
 public:
  CookieControlsContentViewTrackingProtectionUnitTest()
      : view_(std::make_unique<CookieControlsContentView>(
            /*has_act_features=*/true)) {}

  CookieControlsContentViewTrackingProtectionUnitTest(
      const CookieControlsContentViewTrackingProtectionUnitTest&) = delete;
  ~CookieControlsContentViewTrackingProtectionUnitTest() override = default;

 protected:
  ImageModel GetImageModel(const gfx::VectorIcon& icon) {
    return ImageModel::FromVectorIcon(icon, ui::kColorIcon,
                                      GetLayoutConstant(PAGE_INFO_ICON_SIZE));
  }
  views::Label* GetManagedTitle() { return view_->managed_title_; }
  RichControlsContainerView* GetCookiesRow() { return view_->cookies_row_; }
  CookieControlsContentView* GetContentView() { return view_.get(); }

  std::unique_ptr<CookieControlsContentView> view_;
};

namespace {

TEST_F(CookieControlsContentViewTrackingProtectionUnitTest,
       CreateRowForThirdPartyCookiesWithProtectionsOff) {
  content_settings::TrackingProtectionFeature feature = {
      content_settings::TrackingProtectionFeatureType::kThirdPartyCookies,
      CookieControlsEnforcement::kNoEnforcement,
      content_settings::TrackingProtectionBlockingStatus::kLimited};
  GetContentView()->AddFeatureRow(feature, true);

  EXPECT_EQ(GetCookiesRow()->GetTitleForTesting(),
            l10n_util::GetStringUTF16(
                IDS_COOKIE_CONTROLS_BUBBLE_THIRD_PARTY_COOKIES_LABEL));
  EXPECT_EQ(GetCookiesRow()->GetIconForTesting(),
            GetImageModel(views::kEyeCrossedRefreshIcon));
}

TEST_F(CookieControlsContentViewTrackingProtectionUnitTest,
       CreateRowForThirdPartyCookiesWithProtectionsOn) {
  content_settings::TrackingProtectionFeature feature = {
      content_settings::TrackingProtectionFeatureType::kThirdPartyCookies,
      CookieControlsEnforcement::kNoEnforcement,
      content_settings::TrackingProtectionBlockingStatus::kLimited};
  GetContentView()->AddFeatureRow(feature, false);

  EXPECT_EQ(GetCookiesRow()->GetTitleForTesting(),
            l10n_util::GetStringUTF16(
                IDS_COOKIE_CONTROLS_BUBBLE_THIRD_PARTY_COOKIES_LABEL));
  EXPECT_EQ(GetCookiesRow()->GetIconForTesting(),
            GetImageModel(views::kEyeRefreshIcon));
}

TEST_F(CookieControlsContentViewTrackingProtectionUnitTest,
       CreateRowForThirdPartyCookiesWithEnforcementByCookieSetting) {
  content_settings::TrackingProtectionFeature feature = {
      content_settings::TrackingProtectionFeatureType::kThirdPartyCookies,
      CookieControlsEnforcement::kEnforcedByCookieSetting,
      content_settings::TrackingProtectionBlockingStatus::kLimited};
  GetContentView()->AddFeatureRow(feature, /*protections_on=*/false);

  EXPECT_EQ(GetCookiesRow()->GetTitleForTesting(),
            l10n_util::GetStringUTF16(
                IDS_COOKIE_CONTROLS_BUBBLE_THIRD_PARTY_COOKIES_LABEL));
  EXPECT_EQ(GetCookiesRow()->GetIconForTesting(),
            GetImageModel(views::kEyeRefreshIcon));
  EXPECT_EQ(GetCookiesRow()->GetEnforcedIconForTesting(),
            GetImageModel(vector_icons::kSettingsChromeRefreshIcon));
}

TEST_F(CookieControlsContentViewTrackingProtectionUnitTest,
       CreateManagedSectionForCookieSettingEnforcement) {
  GetContentView()->AddManagedSectionForEnforcement(
      CookieControlsEnforcement::kEnforcedByCookieSetting);

  content_settings::TrackingProtectionFeature feature = {
      content_settings::TrackingProtectionFeatureType::kThirdPartyCookies,
      CookieControlsEnforcement::kEnforcedByCookieSetting,
      content_settings::TrackingProtectionBlockingStatus::kLimited};
  GetContentView()->AddFeatureRow(feature, false);

  EXPECT_EQ(GetCookiesRow()->GetTitleForTesting(),
            l10n_util::GetStringUTF16(
                IDS_COOKIE_CONTROLS_BUBBLE_THIRD_PARTY_COOKIES_LABEL));
  EXPECT_EQ(GetManagedTitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_PERMANENT_ALLOWED_TITLE));
  EXPECT_EQ(GetCookiesRow()->GetIconForTesting(),
            GetImageModel(views::kEyeRefreshIcon));
  EXPECT_EQ(GetCookiesRow()->GetEnforcedIconForTesting(),
            GetImageModel(vector_icons::kSettingsChromeRefreshIcon));
}

TEST_F(CookieControlsContentViewTrackingProtectionUnitTest,
       CreateManagedSectionForPolicyEnforcement) {
  GetContentView()->AddManagedSectionForEnforcement(
      CookieControlsEnforcement::kEnforcedByPolicy);

  content_settings::TrackingProtectionFeature feature = {
      content_settings::TrackingProtectionFeatureType::kThirdPartyCookies,
      CookieControlsEnforcement::kEnforcedByPolicy,
      content_settings::TrackingProtectionBlockingStatus::kLimited};
  GetContentView()->AddFeatureRow(feature, false);

  EXPECT_EQ(GetCookiesRow()->GetTitleForTesting(),
            l10n_util::GetStringUTF16(
                IDS_COOKIE_CONTROLS_BUBBLE_THIRD_PARTY_COOKIES_LABEL));
  EXPECT_EQ(GetManagedTitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_MANAGED_PROTECTIONS_LABEL));
  EXPECT_EQ(GetCookiesRow()->GetIconForTesting(),
            GetImageModel(views::kEyeRefreshIcon));
  EXPECT_EQ(GetCookiesRow()->GetEnforcedIconForTesting(),
            GetImageModel(vector_icons::kBusinessChromeRefreshIcon));
}

}  // namespace
