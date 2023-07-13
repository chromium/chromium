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
#include "components/content_settings/core/common/features.h"
#include "components/page_info/page_info.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/test/widget_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user.h"
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

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
class FakeAffiliatedUser : public user_manager::User {
 public:
  explicit FakeAffiliatedUser(const AccountId& account_id) : User(account_id) {
    SetAffiliation(true);
  }

  user_manager::UserType GetType() const override {
    return user_manager::USER_TYPE_REGULAR;
  }
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
    auto account_id =
        AccountId::FromUserEmailGaiaId(profile()->GetProfileUserName(), "id");
    user_ = std::make_unique<FakeAffiliatedUser>(account_id);
    ash::ProfileHelper::Get()->SetProfileToUserMappingForTesting(user_.get());
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user_.get(),
                                                                 profile());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    presenter_ = std::make_unique<PageInfo>(
        std::make_unique<ChromePageInfoDelegate>(web_contents), web_contents,
        url);
    content_view_ =
        std::make_unique<PageInfoCookiesContentView>(presenter_.get());
  }

  PageInfoCookiesContentView* content_view() { return content_view_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<PageInfo> presenter_;
  std::unique_ptr<PageInfoCookiesContentView> content_view_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<FakeAffiliatedUser> user_;
#endif
};

TEST_P(PageInfoCookiesContentViewTest, ThirdPartyCookiesAllowedByDefault) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.blocked_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  cookie_info.status = CookieControlsStatus::kDisabled;
  cookie_info.enforcement = CookieControlsEnforcement::kNoEnforcement;
  cookie_info.expiration = base::Time();
  cookie_info.confidence =
      CookieControlsBreakageConfidenceLevel::kUninitialized;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_FALSE(content_view()
                   ->third_party_cookies_container_for_testing()
                   ->GetVisible());

  // Manage cookies button:
  auto subtitle = GetManageButtonSubtitle(content_view());
  EXPECT_EQ(subtitle, l10n_util::GetPluralStringFUTF16(
                          IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                          cookie_info.allowed_sites_count));
}

TEST_P(PageInfoCookiesContentViewTest, ThirdPartyCookiesBlocked) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.blocked_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  cookie_info.status = CookieControlsStatus::kEnabled;
  cookie_info.enforcement = CookieControlsEnforcement::kNoEnforcement;
  cookie_info.expiration = base::Time();
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(content_view()
                  ->third_party_cookies_container_for_testing()
                  ->GetVisible());

  auto* title_label = content_view()->third_party_cookies_title_for_testing();
  auto* description_label =
      content_view()->third_party_cookies_description_for_testing();

  EXPECT_EQ(
      title_label->GetText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_SITE_NOT_WORKING_TITLE));
  EXPECT_EQ(
      description_label->GetText(),
      l10n_util::GetStringUTF16(
          GetParam()
              ? IDS_PAGE_INFO_COOKIES_SITE_NOT_WORKING_DESCRIPTION_TEMPORARY
              : IDS_PAGE_INFO_COOKIES_SITE_NOT_WORKING_DESCRIPTION_PERMANENT));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_P(PageInfoCookiesContentViewTest, ThirdPartyCookiesAllowedPermanent) {
  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.blocked_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.enforcement = CookieControlsEnforcement::kNoEnforcement;
  cookie_info.expiration = base::Time();
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(content_view()
                  ->third_party_cookies_container_for_testing()
                  ->GetVisible());

  auto* title_label = content_view()->third_party_cookies_title_for_testing();
  auto* description_label =
      content_view()->third_party_cookies_description_for_testing();

  EXPECT_EQ(
      title_label->GetText(),
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_PERMANENT_ALLOWED_TITLE));
  EXPECT_EQ(description_label->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_COOKIES_PERMANENT_ALLOWED_DESCRIPTION));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

TEST_P(PageInfoCookiesContentViewTest, ThirdPartyCookiesAllowedTemporary) {
  const int kDaysToExpiration = 30;

  PageInfoCookiesContentView::CookiesNewInfo cookie_info;
  cookie_info.blocked_sites_count = 3;
  cookie_info.allowed_sites_count = 10;
  cookie_info.status = CookieControlsStatus::kDisabledForSite;
  cookie_info.enforcement = CookieControlsEnforcement::kNoEnforcement;
  cookie_info.expiration = base::Time::Now() + base::Days(kDaysToExpiration);
  cookie_info.confidence = CookieControlsBreakageConfidenceLevel::kMedium;

  content_view()->SetCookieInfo(cookie_info);

  // Third-party cookies section:
  EXPECT_TRUE(content_view()
                  ->third_party_cookies_container_for_testing()
                  ->GetVisible());

  auto* title_label = content_view()->third_party_cookies_title_for_testing();
  auto* description_label =
      content_view()->third_party_cookies_description_for_testing();

  EXPECT_EQ(
      title_label->GetText(),
      l10n_util::GetPluralStringFUTF16(
          IDS_PAGE_INFO_COOKIES_BLOCKING_RESTART_TITLE, kDaysToExpiration));
  EXPECT_EQ(description_label->GetText(),
            l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_COOKIES_BLOCKING_RESTART_DESCRIPTION_TODAY));

  // Manage cookies button:
  EXPECT_EQ(GetManageButtonSubtitle(content_view()),
            l10n_util::GetPluralStringFUTF16(
                IDS_PAGE_INFO_COOKIES_ALLOWED_SITES_COUNT,
                cookie_info.allowed_sites_count));
}

// TODO(crbug.com/1446230): Add tests for enforced cases.

// Runs all tests with two versions of user bypass - one that creates temporary
// exceptions and one that creates permanent exceptions.
INSTANTIATE_TEST_SUITE_P(All, PageInfoCookiesContentViewTest, testing::Bool());
