// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_cookies_content_view.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/features.h"
#include "components/page_info/page_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/vector_icons.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

using Status = ::content_settings::TrackingProtectionBlockingStatus;
using FeatureType = ::content_settings::TrackingProtectionFeatureType;

std::u16string GetManageButtonSubtitle(views::View* content_view) {
  auto* manage_button = content_view->GetViewByID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG);
  EXPECT_TRUE(manage_button);
  auto* managed_button_subtitle =
      static_cast<RichHoverButton*>(manage_button)->GetSubTitleViewForTesting();
  return managed_button_subtitle->GetText();
}

const char* GetVectorIconName(views::ImageView* image_view) {
  return image_view->GetImageModel().GetVectorIcon().vector_icon()->name;
}

PageInfoCookiesContentView::CookiesNewInfo DefaultCookieInfoForTests(
    int days_to_expiration = 0) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.allowed_sites_count = 10;
  // base::Time() represents a null when used as an expiration.
  cookie_info.expiration =
      days_to_expiration ? base::Time::Now() + base::Days(days_to_expiration)
                         : base::Time();
  cookie_info.controls_visible = true;
  cookie_info.protections_on = true;
  cookie_info.enforcement = CookieControlsEnforcement::kNoEnforcement;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kNotIn3pcd;
  cookie_info.is_otr = false;
  cookie_info.features = {{FeatureType::kThirdPartyCookies,
                           CookieControlsEnforcement::kNoEnforcement,
                           Status::kAllowed}};
  return cookie_info;
}

const int kDaysToExpiration = 30;

}  // namespace

class PageInfoCookiesContentViewBaseTestClass : public TestWithBrowserView {
 public:
  PageInfoCookiesContentViewBaseTestClass()
      : TestWithBrowserView(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(EnabledFeatures(), {});
    TestWithBrowserView::SetUp();

    const GURL url("http://a.com");
    AddTab(browser(), url);
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

    presenter_ = std::make_unique<PageInfo>(
        std::make_unique<ChromePageInfoDelegate>(web_contents), web_contents,
        url);
    content_view_ =
        std::make_unique<PageInfoCookiesContentView>(presenter_.get());
  }

  void TearDown() override {
    presenter_.reset();
    content_view_.reset();
    TestWithBrowserView::TearDown();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void LogIn(const std::string& email) override {
    const AccountId account_id = AccountId::FromUserEmail(email);
    user_manager()->AddUserWithAffiliation(account_id, /*is_affiliated=*/true);
    ash_test_helper()->test_session_controller_client()->AddUserSession(email);
    user_manager()->UserLoggedIn(
        account_id,
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
        /*browser_restart=*/false,
        /*is_child=*/false);
  }
#endif

  PageInfoCookiesContentView* content_view() { return content_view_.get(); }

  views::StyledLabel* third_party_cookies_description_label() {
    return content_view_->cookies_description_label_;
  }

  views::BoxLayoutView* third_party_cookies_container() {
    return content_view_->third_party_cookies_container_;
  }

  views::BoxLayoutView* third_party_cookies_label_wrapper() {
    return content_view_->third_party_cookies_label_wrapper_;
  }

  views::Label* third_party_cookies_title() {
    return content_view_->third_party_cookies_title_;
  }

  views::Label* third_party_cookies_description() {
    return content_view_->third_party_cookies_description_;
  }

  views::ToggleButton* third_party_cookies_toggle() {
    return content_view_->third_party_cookies_toggle_;
  }

  views::ImageView* third_party_cookies_enforced_icon() {
    return content_view_->third_party_cookies_enforced_icon_;
  }

  views::Label* third_party_cookies_toggle_subtitle() {
    return content_view_->third_party_cookies_toggle_subtitle_;
  }

  RichControlsContainerView* third_party_cookies_row() {
    return content_view_->third_party_cookies_row_;
  }

  ui::ImageModel GetImageModel(const gfx::VectorIcon& icon) {
    return ui::ImageModel::FromVectorIcon(
        icon, ui::kColorIcon, GetLayoutConstant(PAGE_INFO_ICON_SIZE));
  }

  std::vector<content_settings::TrackingProtectionFeature>
  GetTrackingProtectionFeatures(
      PageInfoCookiesContentView::CookiesNewInfo& cookie_info) {
    if (!cookie_info.protections_on) {
      return {{FeatureType::kThirdPartyCookies, cookie_info.enforcement,
               Status::kAllowed}};
    }
    if (cookie_info.blocking_status == CookieBlocking3pcdStatus::kLimited) {
      return {{FeatureType::kThirdPartyCookies, cookie_info.enforcement,
               Status::kLimited}};
    }
    return {{FeatureType::kThirdPartyCookies, cookie_info.enforcement,
             Status::kBlocked}};
  }

  virtual std::vector<base::test::FeatureRefAndParams> EnabledFeatures() {
    return {};
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<PageInfo> presenter_;
  std::unique_ptr<PageInfoCookiesContentView> content_view_;
};

class PageInfoCookiesContentViewPre3pcdTest
    : public PageInfoCookiesContentViewBaseTestClass {
  std::vector<base::test::FeatureRefAndParams> EnabledFeatures() override {
    return {
        {content_settings::features::kUserBypassUI, {{"expiration", "30d"}}}};
  }
};

TEST_F(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesAllowedByDefault) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.controls_visible = false;
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_FALSE(third_party_cookies_container()->GetVisible());

  // Manage cookies button:
  auto subtitle = GetManageButtonSubtitle(content_view());
  EXPECT_EQ(subtitle, l10n_util::GetPluralStringFUTF16(
                          IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                          cookie_info.allowed_sites_count));
}

TEST_F(PageInfoCookiesContentViewPre3pcdTest, ThirdPartyCookiesBlocked) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();

  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  EXPECT_EQ(
      third_party_cookies_title()->GetText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_SITE_NOT_WORKING_TITLE));
  EXPECT_EQ(
      third_party_cookies_description()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_NOT_WORKING_DESCRIPTION_TEMPORARY));
  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeCrossedRefreshIcon));
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_BLOCKED_SUBTITLE));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_F(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesAllowedPermanent) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.protections_on = false;

  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  EXPECT_EQ(third_party_cookies_title()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_PERMANENT_ALLOWED_TITLE));
  EXPECT_EQ(
      third_party_cookies_description()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_PERMANENT_ALLOWED_DESCRIPTION));
  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeRefreshIcon));
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_F(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesAllowedTemporary) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests(kDaysToExpiration);
  cookie_info.protections_on = false;

  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  EXPECT_EQ(third_party_cookies_title()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_BLOCKED_RESTART_TITLE,
                kDaysToExpiration));
  EXPECT_EQ(third_party_cookies_description()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_RESTART_DESCRIPTION));
  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeRefreshIcon));
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_F(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesBlockedByPolicy) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByPolicy;

  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeCrossedRefreshIcon));
  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetIsOn());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_BLOCKED_SUBTITLE));

  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());
  EXPECT_STREQ(GetVectorIconName(third_party_cookies_enforced_icon()),
               vector_icons::kBusinessChromeRefreshIcon.name);
  EXPECT_EQ(
      third_party_cookies_enforced_icon()->GetTooltipText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_MANAGED_BY_POLICY));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_F(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesAllowedByPolicy) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.protections_on = false;
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByPolicy;

  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeRefreshIcon));
  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_toggle()->GetIsOn());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE));

  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());
  EXPECT_STREQ(GetVectorIconName(third_party_cookies_enforced_icon()),
               vector_icons::kBusinessChromeRefreshIcon.name);
  EXPECT_EQ(
      third_party_cookies_enforced_icon()->GetTooltipText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_MANAGED_BY_POLICY));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_F(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesBlockedByExtension) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByExtension;

  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeCrossedRefreshIcon));
  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetIsOn());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_BLOCKED_SUBTITLE));

  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());
  EXPECT_STREQ(GetVectorIconName(third_party_cookies_enforced_icon()),
               vector_icons::kExtensionChromeRefreshIcon.name);
  EXPECT_EQ(
      third_party_cookies_enforced_icon()->GetTooltipText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_MANAGED_BY_EXTENSION));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_F(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesAllowedByExtension) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.protections_on = false;
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByExtension;

  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeRefreshIcon));
  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_toggle()->GetIsOn());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE));

  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());
  EXPECT_STREQ(GetVectorIconName(third_party_cookies_enforced_icon()),
               vector_icons::kExtensionChromeRefreshIcon.name);
  EXPECT_EQ(
      third_party_cookies_enforced_icon()->GetTooltipText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_MANAGED_BY_EXTENSION));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_F(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesBlockedBySetting) {
  // This is not be possible, but the UI still should be able to handle this
  // state correctly.
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByCookieSetting;

  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeCrossedRefreshIcon));
  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetIsOn());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_BLOCKED_SUBTITLE));

  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());
  EXPECT_STREQ(GetVectorIconName(third_party_cookies_enforced_icon()),
               vector_icons::kSettingsChromeRefreshIcon.name);
  EXPECT_EQ(
      third_party_cookies_enforced_icon()->GetTooltipText(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_MANAGED_BY_SETTINGS_TOOLTIP));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_F(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesAllowedBySetting) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.protections_on = false;
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByCookieSetting;

  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeRefreshIcon));
  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_toggle()->GetIsOn());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE));

  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());
  EXPECT_STREQ(GetVectorIconName(third_party_cookies_enforced_icon()),
               vector_icons::kSettingsChromeRefreshIcon.name);
  EXPECT_EQ(
      third_party_cookies_enforced_icon()->GetTooltipText(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_BLOCK_THIRD_PARTY_COOKIES_MANAGED_BY_SETTINGS_TOOLTIP));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

class PageInfoCookiesContentView3pcdTitleAndDescriptionTest
    : public PageInfoCookiesContentViewBaseTestClass,
      public testing::WithParamInterface<CookieBlocking3pcdStatus> {};

TEST_F(PageInfoCookiesContentView3pcdTitleAndDescriptionTest,
       DisplaysTitleAndDescriptionWhenCookiesLimitedWithTemporaryException) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests(kDaysToExpiration);
  cookie_info.protections_on = false;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kLimited;
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);

  content_view()->SetCookieInfo(cookie_info);

  EXPECT_EQ(third_party_cookies_title()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_LIMITED_RESTART_TITLE,
                kDaysToExpiration));
  EXPECT_EQ(third_party_cookies_description()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_RESTART_DESCRIPTION));
}

TEST_F(PageInfoCookiesContentView3pcdTitleAndDescriptionTest,
       DisplaysTitleAndDescriptionWhenCookiesBlockedWithTemporaryException) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests(kDaysToExpiration);
  cookie_info.protections_on = false;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kAll;
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);

  content_view()->SetCookieInfo(cookie_info);

  EXPECT_EQ(third_party_cookies_title()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_BLOCKED_RESTART_TITLE,
                kDaysToExpiration));
  EXPECT_EQ(third_party_cookies_description()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_RESTART_DESCRIPTION));
}

TEST_F(PageInfoCookiesContentView3pcdTitleAndDescriptionTest,
       DisplaysBubbleDescriptionLabelWhenCookiesLimited) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kLimited;
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_EQ(third_party_cookies_description_label()->GetText(),
            l10n_util::GetStringFUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_DESCRIPTION,
                l10n_util::GetStringUTF16(
                    IDS_PAGE_INFO_TRACKING_PROTECTION_SETTINGS_LINK)));
}

TEST_F(PageInfoCookiesContentView3pcdTitleAndDescriptionTest,
       DisplaysBubbleDescriptionLabelWhenCookiesBlocked) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kAll;
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_EQ(third_party_cookies_description_label()->GetText(),
            l10n_util::GetStringFUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_BLOCKED_COOKIES_DESCRIPTION,
                l10n_util::GetStringUTF16(
                    IDS_PAGE_INFO_TRACKING_PROTECTION_SETTINGS_LINK)));
}

TEST_F(PageInfoCookiesContentView3pcdTitleAndDescriptionTest,
       DisplaysLabelsWhenCookiesBlockedInIncognitoMode) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kAll;
  cookie_info.is_otr = true;
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_EQ(
      third_party_cookies_title()->GetText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_SITE_NOT_WORKING_TITLE));
  EXPECT_EQ(
      third_party_cookies_description()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_NOT_WORKING_DESCRIPTION_TEMPORARY));
  EXPECT_EQ(
      third_party_cookies_description_label()->GetText(),
      l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_INCOGNITO_BLOCKED_COOKIES_DESCRIPTION,
          l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_TRACKING_PROTECTION_SETTINGS_LINK)));
}

TEST_F(PageInfoCookiesContentView3pcdTitleAndDescriptionTest,
       DisplaysLabelsWhenCookiesAllowedInIncognitoMode) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.protections_on = false;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kAll;
  cookie_info.is_otr = true;
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_EQ(third_party_cookies_title()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_PERMANENT_ALLOWED_TITLE));
  EXPECT_EQ(
      third_party_cookies_description()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_PERMANENT_ALLOWED_DESCRIPTION));
  EXPECT_EQ(
      third_party_cookies_description_label()->GetText(),
      l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_INCOGNITO_BLOCKED_COOKIES_DESCRIPTION,
          l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_TRACKING_PROTECTION_SETTINGS_LINK)));
}

TEST_F(PageInfoCookiesContentView3pcdTitleAndDescriptionTest,
       DisplaysDescriptionWhenCookiesAllowedEnforcedByTpcdGrant) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.protections_on = false;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kLimited;
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByTpcdGrant;
  cookie_info.controls_visible = false;
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_EQ(third_party_cookies_description_label()->GetText(),
            l10n_util::GetStringFUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_GRANT_DESCRIPTION,
                l10n_util::GetStringUTF16(
                    IDS_PAGE_INFO_TRACKING_PROTECTION_SETTINGS_LINK)));
}

TEST_P(PageInfoCookiesContentView3pcdTitleAndDescriptionTest,
       DisplaysTitleAndDescriptionWhenCookiesBlocked) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests(kDaysToExpiration);
  cookie_info.blocking_status = GetParam();
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_EQ(
      third_party_cookies_title()->GetText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_SITE_NOT_WORKING_TITLE));
  EXPECT_EQ(
      third_party_cookies_description()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_SITE_NOT_WORKING_DESCRIPTION_TEMPORARY));
}

TEST_P(PageInfoCookiesContentView3pcdTitleAndDescriptionTest,
       DisplaysTitleAndDescriptionWhenCookiesAllowedWithPermanentException) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.protections_on = false;
  cookie_info.blocking_status = GetParam();
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_EQ(third_party_cookies_title()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_PERMANENT_ALLOWED_TITLE));
  EXPECT_EQ(
      third_party_cookies_description()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_PERMANENT_ALLOWED_DESCRIPTION));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PageInfoCookiesContentView3pcdTitleAndDescriptionTest,
                         testing::Values(CookieBlocking3pcdStatus::kLimited,
                                         CookieBlocking3pcdStatus::kAll));

class PageInfoCookiesContentView3pcdCookieToggleTest
    : public PageInfoCookiesContentViewBaseTestClass,
      public testing::WithParamInterface<CookieBlocking3pcdStatus> {};

TEST_F(PageInfoCookiesContentView3pcdCookieToggleTest,
       DisplaysOffToggleWhenCookiesLimited) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kLimited;
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeCrossedRefreshIcon));
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_LIMITED_SUBTITLE));
}

TEST_F(PageInfoCookiesContentView3pcdCookieToggleTest,
       DisplaysOffToggleWhenCookiesBlocked) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kAll;
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeCrossedRefreshIcon));
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_BLOCKED_SUBTITLE));
}

TEST_F(PageInfoCookiesContentView3pcdCookieToggleTest,
       DisplaysOffToggleWhenCookiesBlockedInIncognitoMode) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kAll;
  cookie_info.is_otr = true;
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeCrossedRefreshIcon));
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_BLOCKED_SUBTITLE));
}

TEST_F(PageInfoCookiesContentView3pcdCookieToggleTest,
       DisplaysOnToggleWhenCookiesAllowedInIncognitoMode) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.protections_on = false;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kAll;
  cookie_info.is_otr = true;
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeRefreshIcon));
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE));
}

TEST_P(PageInfoCookiesContentView3pcdCookieToggleTest,
       DisplaysOnToggleWhenCookiesAllowed) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.blocking_status = GetParam();
  cookie_info.protections_on = false;
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeRefreshIcon));
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE));
}

TEST_P(PageInfoCookiesContentView3pcdCookieToggleTest,
       LabelAndEnforcementShownWhenCookiesAllowedEnforcedBySetting) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByCookieSetting;
  cookie_info.protections_on = false;
  cookie_info.blocking_status = GetParam();
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeRefreshIcon));
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_title()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_PERMANENT_ALLOWED_TITLE));
  EXPECT_EQ(
      third_party_cookies_description()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_PERMANENT_ALLOWED_DESCRIPTION));
  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE));
}

TEST_P(PageInfoCookiesContentView3pcdCookieToggleTest,
       LabelHiddenAndEnforcementShownWhenCookiesAllowedEnforcedByPolicy) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByPolicy;
  cookie_info.protections_on = false;
  cookie_info.blocking_status = GetParam();
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeRefreshIcon));
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE));
}

TEST_P(PageInfoCookiesContentView3pcdCookieToggleTest,
       LabelHiddenAndEnforcementShownWhenCookiesAllowedEnforcedByExtension) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByExtension;
  cookie_info.protections_on = false;
  cookie_info.blocking_status = GetParam();
  cookie_info.features = GetTrackingProtectionFeatures(cookie_info);
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_EQ(third_party_cookies_row()->GetIconForTesting(),
            GetImageModel(views::kEyeRefreshIcon));
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_TRACKING_PROTECTION_BUBBLE_3PC_ALLOWED_SUBTITLE));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PageInfoCookiesContentView3pcdCookieToggleTest,
                         testing::Values(CookieBlocking3pcdStatus::kLimited,
                                         CookieBlocking3pcdStatus::kAll));
