// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_cookies_content_view.h"

#include <memory>
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/features.h"
#include "components/page_info/page_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/test/widget_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

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
  cookie_info.blocked_third_party_sites_count = 8;
  cookie_info.allowed_third_party_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  // base::Time() represents a null when used as an expiration.
  cookie_info.expiration =
      days_to_expiration ? base::Time::Now() + base::Days(days_to_expiration)
                         : base::Time();
  cookie_info.status = CookieControlsStatus::kEnabled;
  cookie_info.enforcement = CookieControlsEnforcement::kNoEnforcement;
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kNotIn3pcd;
  cookie_info.is_otr = false;
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_user_manager_->AddUserWithAffiliation(
        AccountId::FromUserEmail(profile()->GetProfileUserName()),
        /*is_affiliated=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

  virtual std::vector<base::test::FeatureRefAndParams> EnabledFeatures() {
    return {};
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<PageInfo> presenter_;
  std::unique_ptr<PageInfoCookiesContentView> content_view_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
#endif
};

class PageInfoCookiesContentViewPre3pcdTest
    : public PageInfoCookiesContentViewBaseTestClass,
      public testing::WithParamInterface<bool> {
  std::vector<base::test::FeatureRefAndParams> EnabledFeatures() override {
    // Permanent exceptions are represented as expiration = 0d.
    std::string expiration = GetParam() ? "30d" : "0d";
    return {{content_settings::features::kUserBypassUI,
             {{"expiration", expiration}}}};
  }
};

TEST_P(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesAllowedByDefault) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.confidence =
      CookieControlsBreakageConfidenceLevel::kUninitialized;
  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_FALSE(third_party_cookies_container()->GetVisible());

  // Manage cookies button:
  auto subtitle = GetManageButtonSubtitle(content_view());
  EXPECT_EQ(subtitle, l10n_util::GetPluralStringFUTF16(
                          IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                          cookie_info.allowed_sites_count));
}

TEST_P(PageInfoCookiesContentViewPre3pcdTest, ThirdPartyCookiesBlocked) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  EXPECT_EQ(
      third_party_cookies_title()->GetText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_SITE_NOT_WORKING_TITLE));
  EXPECT_EQ(
      third_party_cookies_description()->GetText(),
      l10n_util::GetStringUTF16(
          GetParam()
              ? IDS_PAGE_INFO_COOKIES_SITE_NOT_WORKING_DESCRIPTION_TEMPORARY
              : IDS_PAGE_INFO_COOKIES_SITE_NOT_WORKING_DESCRIPTION_PERMANENT));
  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_BLOCKED_SITES_COUNT,
                cookie_info.blocked_third_party_sites_count));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_P(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesAllowedPermanent) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  EXPECT_EQ(
      third_party_cookies_title()->GetText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_PERMANENT_ALLOWED_TITLE));
  EXPECT_EQ(third_party_cookies_description()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_COOKIES_PERMANENT_ALLOWED_DESCRIPTION));
  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_third_party_sites_count));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_P(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesAllowedTemporary) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests(kDaysToExpiration);
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  EXPECT_EQ(
      third_party_cookies_title()->GetText(),
      l10n_util::GetPluralStringFUTF16(
          IDS_PAGE_INFO_COOKIES_BLOCKING_RESTART_TITLE, kDaysToExpiration));
  EXPECT_EQ(third_party_cookies_description()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_COOKIES_BLOCKING_RESTART_DESCRIPTION_TODAY));
  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_third_party_sites_count));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_P(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesBlockedByPolicy) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByPolicy;
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetIsOn());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_BLOCKED_SITES_COUNT,
                cookie_info.blocked_third_party_sites_count));

  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());
  EXPECT_STREQ(GetVectorIconName(third_party_cookies_enforced_icon()),
               vector_icons::kBusinessIcon.name);
  EXPECT_EQ(
      third_party_cookies_enforced_icon()->GetTooltipText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_MANAGED_BY_POLICY));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_P(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesAllowedByPolicy) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByPolicy;
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  // TODO(http://b/308988593): Verify that the toggle row has correct subtitle.
  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_toggle()->GetIsOn());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_third_party_sites_count));

  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());
  EXPECT_STREQ(GetVectorIconName(third_party_cookies_enforced_icon()),
               vector_icons::kBusinessIcon.name);
  EXPECT_EQ(
      third_party_cookies_enforced_icon()->GetTooltipText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_MANAGED_BY_POLICY));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_P(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesBlockedByExtension) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByExtension;
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetIsOn());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_BLOCKED_SITES_COUNT,
                cookie_info.blocked_third_party_sites_count));

  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());
  EXPECT_STREQ(GetVectorIconName(third_party_cookies_enforced_icon()),
               vector_icons::kExtensionIcon.name);
  EXPECT_EQ(
      third_party_cookies_enforced_icon()->GetTooltipText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_MANAGED_BY_EXTENSION));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_P(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesAllowedByExtension) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByExtension;
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_toggle()->GetIsOn());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_third_party_sites_count));

  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());
  EXPECT_STREQ(GetVectorIconName(third_party_cookies_enforced_icon()),
               vector_icons::kExtensionIcon.name);
  EXPECT_EQ(
      third_party_cookies_enforced_icon()->GetTooltipText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_PERMISSION_MANAGED_BY_EXTENSION));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_P(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesBlockedBySetting) {
  // This is not be possible, but the UI still should be able to handle this
  // state correctly.
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByCookieSetting;
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetIsOn());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_BLOCKED_SITES_COUNT,
                cookie_info.blocked_third_party_sites_count));

  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());
  EXPECT_STREQ(GetVectorIconName(third_party_cookies_enforced_icon()),
               vector_icons::kSettingsIcon.name);
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

TEST_P(PageInfoCookiesContentViewPre3pcdTest,
       ThirdPartyCookiesAllowedBySetting) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByCookieSetting;
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_toggle()->GetIsOn());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_third_party_sites_count));

  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());
  EXPECT_STREQ(GetVectorIconName(third_party_cookies_enforced_icon()),
               vector_icons::kSettingsIcon.name);
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

// Runs all tests with two versions of user bypass - one that creates temporary
// exceptions and one that creates permanent exceptions.
INSTANTIATE_TEST_SUITE_P(All,
                         PageInfoCookiesContentViewPre3pcdTest,
                         testing::Bool());

class PageInfoCookiesContentView3pcdTitleAndDescriptionTest
    : public PageInfoCookiesContentViewBaseTestClass,
      public testing::WithParamInterface<CookieBlocking3pcdStatus> {};

TEST_F(PageInfoCookiesContentView3pcdTitleAndDescriptionTest,
       DisplaysTitleAndDescriptionWhenCookiesLimitedWithTemporaryException) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests(kDaysToExpiration);
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kLimited;
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_EQ(
      third_party_cookies_title()->GetText(),
      l10n_util::GetPluralStringFUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_LIMITING_RESTART_TITLE,
          kDaysToExpiration));
  EXPECT_EQ(
      third_party_cookies_description()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_COOKIES_TRACKING_PROTECTION_COOKIES_RESTART_DESCRIPTION));
}

TEST_F(PageInfoCookiesContentView3pcdTitleAndDescriptionTest,
       DisplaysTitleAndDescriptionWhenCookiesBlockedWithTemporaryException) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests(kDaysToExpiration);
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kAll;
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_EQ(
      third_party_cookies_title()->GetText(),
      l10n_util::GetPluralStringFUTF16(
          IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_3PC_BLOCKED_RESTART_TITLE,
          kDaysToExpiration));
  EXPECT_EQ(
      third_party_cookies_description()->GetText(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_COOKIES_TRACKING_PROTECTION_COOKIES_RESTART_DESCRIPTION));
}

TEST_F(PageInfoCookiesContentView3pcdTitleAndDescriptionTest,
       DisplaysBubbleDescriptionLabelWhenCookiesLimited) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kLimited;
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
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kAll;
  cookie_info.is_otr = true;
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
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kLimited;
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByTpcdGrant;
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
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.blocking_status = GetParam();
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
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_LIMITED));
}

TEST_F(PageInfoCookiesContentView3pcdCookieToggleTest,
       DisplaysOffToggleWhenCookiesBlocked) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kAll;
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_BLOCKED));
}

TEST_F(PageInfoCookiesContentView3pcdCookieToggleTest,
       DisplaysOffToggleWhenCookiesBlockedInIncognitoMode) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kAll;
  cookie_info.is_otr = true;
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_FALSE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_BLOCKED));
}

TEST_F(PageInfoCookiesContentView3pcdCookieToggleTest,
       DisplaysOnToggleWhenCookiesAllowedInIncognitoMode) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kAll;
  cookie_info.is_otr = true;
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_ALLOWED));
}

TEST_P(PageInfoCookiesContentView3pcdCookieToggleTest,
       DisplaysOnToggleWhenCookiesAllowed) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.blocking_status = GetParam();
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_TRUE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_toggle()->GetIsOn());
  EXPECT_FALSE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_ALLOWED));
}

TEST_P(PageInfoCookiesContentView3pcdCookieToggleTest,
       LabelAndEnforcementShownWhenCookiesAllowedEnforcedBySetting) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByCookieSetting;
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.blocking_status = GetParam();
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_TRUE(third_party_cookies_label_wrapper()->GetVisible());
  // TODO(http://b/308988593): Verify the toggle row icon.
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
                IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_ALLOWED));
}

TEST_P(PageInfoCookiesContentView3pcdCookieToggleTest,
       LabelHiddenAndEnforcementShownWhenCookiesAllowedEnforcedByPolicy) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByPolicy;
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.blocking_status = GetParam();
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_ALLOWED));
}

TEST_P(PageInfoCookiesContentView3pcdCookieToggleTest,
       LabelHiddenAndEnforcementShownWhenCookiesAllowedEnforcedByExtension) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info =
      DefaultCookieInfoForTests();
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByExtension;
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.blocking_status = GetParam();
  content_view()->SetCookieInfo(cookie_info);

  EXPECT_FALSE(third_party_cookies_label_wrapper()->GetVisible());
  // TODO(http://b/308988593): Verify the toggle row icon.
  EXPECT_FALSE(third_party_cookies_toggle()->GetVisible());
  EXPECT_TRUE(third_party_cookies_enforced_icon()->GetVisible());

  EXPECT_EQ(third_party_cookies_toggle_subtitle()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_TRACKING_PROTECTION_COOKIES_ALLOWED));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PageInfoCookiesContentView3pcdCookieToggleTest,
                         testing::Values(CookieBlocking3pcdStatus::kLimited,
                                         CookieBlocking3pcdStatus::kAll));
