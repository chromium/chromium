// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "content/public/browser/cookie_access_details.h"

namespace {

const char kCurrentUrl[] = "https://google.com";
const char kThirdPartyUrl[] = "https://youtube.com";

}  // namespace

class PageSpecificSiteDataDialogUnitTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PageSpecificSiteDataDialogUnitTest() = default;

  PageSpecificSiteDataDialogUnitTest(
      const PageSpecificSiteDataDialogUnitTest&) = delete;
  PageSpecificSiteDataDialogUnitTest& operator=(
      const PageSpecificSiteDataDialogUnitTest&) = delete;

  ~PageSpecificSiteDataDialogUnitTest() override = default;

  content_settings::PageSpecificContentSettings* GetContentSettings() {
    return content_settings::PageSpecificContentSettings::GetForFrame(
        main_rfh());
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL(kCurrentUrl));
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
            web_contents()));
  }
};

TEST_F(PageSpecificSiteDataDialogUnitTest, CookieAccessed) {
  // Verify that site data access through CookiesTreeModel is correctly
  // displayed in the dialog.
  auto* content_settings = GetContentSettings();

  std::unique_ptr<net::CanonicalCookie> first_party_cookie(
      net::CanonicalCookie::Create(GURL(kCurrentUrl), "A=B", base::Time::Now(),
                                   absl::nullopt /* server_time */,
                                   absl::nullopt /* cookie_partition_key */));
  std::unique_ptr<net::CanonicalCookie> third_party_cookie(
      net::CanonicalCookie::Create(GURL(kThirdPartyUrl), "C=D",
                                   base::Time::Now(),
                                   absl::nullopt /* server_time */,
                                   absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(first_party_cookie);
  ASSERT_TRUE(third_party_cookie);
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kCurrentUrl),
       GURL(kCurrentUrl),
       {*first_party_cookie},
       false});
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kThirdPartyUrl),
       /*firstparty*/ GURL(kCurrentUrl),
       {*third_party_cookie},
       false});

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto sites = delegate->GetAllSites();
  ASSERT_EQ(sites.size(), 2u);

  auto first_site = sites[0];
  EXPECT_EQ(first_site.origin.host(), GURL(kCurrentUrl).host());
  EXPECT_EQ(first_site.setting, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(first_site.is_fully_partitioned, false);

  auto second_site = sites[1];
  EXPECT_EQ(second_site.origin.host(), GURL(kThirdPartyUrl).host());
  EXPECT_EQ(second_site.setting, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(second_site.is_fully_partitioned, false);
}

TEST_F(PageSpecificSiteDataDialogUnitTest, TrustTokenAccessed) {
  // Verify that site data access through BrowsingDataModel is correctly
  // displayed in the dialog.]
  auto* content_settings = GetContentSettings();

  content_settings->OnTrustTokenAccessed(
      url::Origin::Create(GURL(kThirdPartyUrl)),
      /*blocked=*/false);

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto sites = delegate->GetAllSites();
  ASSERT_EQ(sites.size(), 1u);
  auto first_site = sites[0];
  EXPECT_EQ(first_site.origin.host(), GURL(kThirdPartyUrl).host());
  EXPECT_EQ(first_site.setting, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(first_site.is_fully_partitioned, false);
}

TEST_F(PageSpecificSiteDataDialogUnitTest, MixedModelAccess) {
  // Verify that site data access through CookiesTreeModel and BrowsingDataModel
  // is correctly displayed in the dialog.
  auto* content_settings = GetContentSettings();

  std::unique_ptr<net::CanonicalCookie> third_party_cookie(
      net::CanonicalCookie::Create(GURL(kThirdPartyUrl), "C=D",
                                   base::Time::Now(),
                                   absl::nullopt /* server_time */,
                                   absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(third_party_cookie);
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kThirdPartyUrl),
       /*firstparty*/ GURL(kCurrentUrl),
       {*third_party_cookie},
       false});
  content_settings->OnTrustTokenAccessed(
      url::Origin::Create(GURL(kThirdPartyUrl)),
      /*blocked=*/false);

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto sites = delegate->GetAllSites();
  // kThirdPartyUrl has accessed two types of site data and it's being reported
  // through two models: CookieTreeModel and BrowsingDataModel. It should be
  // combined into single entry in the site data dialog.
  ASSERT_EQ(sites.size(), 1u);

  auto first_site = sites[0];
  EXPECT_EQ(first_site.origin.host(), GURL(kThirdPartyUrl).host());
  EXPECT_EQ(first_site.setting, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(first_site.is_fully_partitioned, false);
}
