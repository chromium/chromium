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

}  // namespace

class PageInfoCookiesContentViewTest
    : public TestWithBrowserView,
      public testing::WithParamInterface<bool> {
 public:
  PageInfoCookiesContentViewTest()
      : TestWithBrowserView(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    std::string expiration = GetParam() ? "30d" : "0d";
    feature_list_.InitWithFeaturesAndParameters(
        {{content_settings::features::kUserBypassUI,
          {{"expiration", expiration}}}},
        {});
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

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<PageInfo> presenter_;
  std::unique_ptr<PageInfoCookiesContentView> content_view_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
#endif
};

TEST_P(PageInfoCookiesContentViewTest, ThirdPartyCookiesAllowedByDefault) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.blocked_third_party_sites_count = 8;
  cookie_info.allowed_third_party_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  cookie_info.status = CookieControlsStatus::kDisabled;
  cookie_info.enforcement = CookieControlsEnforcement::kNoEnforcement;
  cookie_info.expiration = base::Time();
  cookie_info.confidence =
      CookieControlsBreakageConfidenceLevel::kUninitialized;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kNotIn3pcd;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_FALSE(third_party_cookies_container()->GetVisible());

  // Manage cookies button:
  auto subtitle = GetManageButtonSubtitle(content_view());
  EXPECT_EQ(subtitle, l10n_util::GetPluralStringFUTF16(
                          IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                          cookie_info.allowed_sites_count));
}

TEST_P(PageInfoCookiesContentViewTest, ThirdPartyCookiesBlocked) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.blocked_third_party_sites_count = 8;
  cookie_info.allowed_third_party_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  cookie_info.status = CookieControlsStatus::kEnabled;
  cookie_info.enforcement = CookieControlsEnforcement::kNoEnforcement;
  cookie_info.expiration = base::Time();
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kNotIn3pcd;

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
  // TODO(crbug.com/1446230): Verify the toggle row icon.
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

TEST_P(PageInfoCookiesContentViewTest, ThirdPartyCookiesAllowedPermanent) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.blocked_third_party_sites_count = 8;
  cookie_info.allowed_third_party_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.enforcement = CookieControlsEnforcement::kNoEnforcement;
  cookie_info.expiration = base::Time();
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kNotIn3pcd;

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
  // TODO(crbug.com/1446230): Verify the toggle row icon.
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

TEST_P(PageInfoCookiesContentViewTest, ThirdPartyCookiesAllowedTemporary) {
  const int kDaysToExpiration = 30;

  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.blocked_third_party_sites_count = 8;
  cookie_info.allowed_third_party_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.enforcement = CookieControlsEnforcement::kNoEnforcement;
  cookie_info.expiration = base::Time::Now() + base::Days(kDaysToExpiration);
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kNotIn3pcd;

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
  // TODO(crbug.com/1446230): Verify the toggle row icon.
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

TEST_P(PageInfoCookiesContentViewTest, ThirdPartyCookiesBlockedByPolicy) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.blocked_third_party_sites_count = 8;
  cookie_info.allowed_third_party_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  cookie_info.status = CookieControlsStatus::kEnabled;
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByPolicy;
  cookie_info.expiration = base::Time();
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kNotIn3pcd;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  // TODO(crbug.com/1446230): Verify the toggle row icon.
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

TEST_P(PageInfoCookiesContentViewTest, ThirdPartyCookiesAllowedByPolicy) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.blocked_third_party_sites_count = 8;
  cookie_info.allowed_third_party_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByPolicy;
  cookie_info.expiration = base::Time();
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kNotIn3pcd;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  // TODO(crbug.com/1446230): Verify that the toggle row has correct subtitle.
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

TEST_P(PageInfoCookiesContentViewTest, ThirdPartyCookiesBlockedByExtension) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.blocked_third_party_sites_count = 8;
  cookie_info.allowed_third_party_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  cookie_info.status = CookieControlsStatus::kEnabled;
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByExtension;
  cookie_info.expiration = base::Time();
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kNotIn3pcd;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  // TODO(crbug.com/1446230): Verify the toggle row icon.
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

TEST_P(PageInfoCookiesContentViewTest, ThirdPartyCookiesAllowedByExtension) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.blocked_third_party_sites_count = 8;
  cookie_info.allowed_third_party_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByExtension;
  cookie_info.expiration = base::Time();
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kNotIn3pcd;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  // TODO(crbug.com/1446230): Verify the toggle row icon.
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

TEST_P(PageInfoCookiesContentViewTest, ThirdPartyCookiesBlockedBySetting) {
  // This is not be possible, but the UI still should be able to handle this
  // state correctly.
  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.blocked_third_party_sites_count = 8;
  cookie_info.allowed_third_party_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  cookie_info.status = CookieControlsStatus::kEnabled;
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByCookieSetting;
  cookie_info.expiration = base::Time();
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kNotIn3pcd;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  // TODO(crbug.com/1446230): Verify the toggle row icon.
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

TEST_P(PageInfoCookiesContentViewTest, ThirdPartyCookiesAllowedBySetting) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.blocked_third_party_sites_count = 8;
  cookie_info.allowed_third_party_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.enforcement = CookieControlsEnforcement::kEnforcedByCookieSetting;
  cookie_info.expiration = base::Time();
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;
  cookie_info.blocking_status = CookieBlocking3pcdStatus::kNotIn3pcd;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(third_party_cookies_container()->GetVisible());

  // TODO(crbug.com/1446230): Verify the toggle row icon.
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
INSTANTIATE_TEST_SUITE_P(All, PageInfoCookiesContentViewTest, testing::Bool());
