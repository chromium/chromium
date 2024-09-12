// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"

#include "base/feature_list.h"
#include "base/test/bind.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/browsing_data/content/fake_browsing_data_model.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/common/content_settings_manager.mojom.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/cookie_access_details.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using StorageType =
    content_settings::mojom::ContentSettingsManager::StorageType;
using ::testing::Contains;
using ::testing::Not;

const char kCurrentUrl[] = "https://google.com";
const char kCurrentUrlSubPage[] = "https://google.com/subpage";
const char kThirdPartyUrl[] = "https://youtube.com";
const char kExampleUrl[] = "https://example.com";

void ValidateAllowedUnpartitionedSites(
    test::PageSpecificSiteDataDialogTestApi* delegate,
    const std::vector<GURL>& expected_sites_in_order) {
  auto sites = delegate->GetAllSites();
  ASSERT_EQ(sites.size(), expected_sites_in_order.size());

  // All sites should be allowed and not fully partitioned.
  for (auto& site : sites) {
    EXPECT_EQ(site.setting, CONTENT_SETTING_ALLOW);
    EXPECT_FALSE(site.is_fully_partitioned);
  }

  // Hosts should match in order.
  EXPECT_TRUE(
      base::ranges::equal(sites, expected_sites_in_order,
                          [](const auto& site, const auto& expected_site) {
                            return site.origin.host() == expected_site.host();
                          }));
}

blink::StorageKey CreateUnpartitionedStorageKey(const GURL& url) {
  return blink::StorageKey::CreateFirstParty(url::Origin::Create(url));
}

blink::StorageKey CreateThirdPartyStorageKey(const GURL& url,
                                             const GURL& top_url) {
  return blink::StorageKey::Create(
      url::Origin::Create(url),
      net::SchemefulSite(url::Origin::Create(top_url)),
      blink::mojom::AncestorChainBit::kCrossSite,
      /*third_party_partitioning_allowed=*/true);
}

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
        std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));

    profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
  }
};

TEST_F(PageSpecificSiteDataDialogUnitTest, CookieAccessed) {
  // Verify that site data access through BrowsingDataModel is correctly
  // displayed in the dialog.
  auto* content_settings = GetContentSettings();

  std::unique_ptr<net::CanonicalCookie> first_party_cookie(
      net::CanonicalCookie::CreateForTesting(GURL(kCurrentUrl), "A=B",
                                             base::Time::Now()));
  std::unique_ptr<net::CanonicalCookie> third_party_cookie(
      net::CanonicalCookie::CreateForTesting(GURL(kThirdPartyUrl), "C=D",
                                             base::Time::Now()));
  ASSERT_TRUE(first_party_cookie);
  ASSERT_TRUE(third_party_cookie);
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kCurrentUrl),
       GURL(kCurrentUrl),
       {{*first_party_cookie}},
       /* blocked_by_policy = */ false});
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kThirdPartyUrl),
       /*firstparty*/ GURL(kCurrentUrl),
       {{*third_party_cookie}},
       /* blocked_by_policy = */ false});

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  ValidateAllowedUnpartitionedSites(delegate.get(),
                                    {GURL(kCurrentUrl), GURL(kThirdPartyUrl)});
}

TEST_F(PageSpecificSiteDataDialogUnitTest, QuotaStorageAccessedFirstParty) {
  // Verify that storage access through BrowsingDataModel is correctly displayed
  // in the dialog.
  auto* content_settings = GetContentSettings();
  content_settings->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL(kThirdPartyUrl)),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto sites = delegate->GetAllSites();
  ASSERT_EQ(sites.size(), 1u);

  auto first_site = sites[0];
  EXPECT_EQ(first_site.origin.host(), GURL(kThirdPartyUrl).host());
  EXPECT_EQ(first_site.setting, CONTENT_SETTING_ALLOW);
  // False due to first-party storage being accessed.
  EXPECT_EQ(first_site.is_fully_partitioned, false);
}

TEST_F(PageSpecificSiteDataDialogUnitTest,
       QuotaStorageAndCookieAccessedFirstParty) {
  // Verify that storage access through BrowsingDataModel is correctly displayed
  // in the dialog.
  auto* content_settings = GetContentSettings();
  std::unique_ptr<net::CanonicalCookie> first_party_cookie(
      net::CanonicalCookie::CreateForTesting(GURL(kThirdPartyUrl), "C=D",
                                             base::Time::Now()));
  ASSERT_TRUE(first_party_cookie);
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kThirdPartyUrl),
       GURL(kThirdPartyUrl),
       {{*first_party_cookie}},
       /* blocked_by_policy = */ false});
  content_settings->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL(kThirdPartyUrl)),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto sites = delegate->GetAllSites();
  ASSERT_EQ(sites.size(), 1u);

  auto first_site = sites[0];
  EXPECT_EQ(first_site.origin.host(), GURL(kThirdPartyUrl).host());
  EXPECT_EQ(first_site.setting, CONTENT_SETTING_ALLOW);
  // False due to first-party storage being accessed.
  EXPECT_EQ(first_site.is_fully_partitioned, false);
}

TEST_F(PageSpecificSiteDataDialogUnitTest,
       QuotaStorageAndPartitionedCookieAccessedFirstParty) {
  // Verify that storage access through BrowsingDataModel is correctly displayed
  // in the dialog.
  auto* content_settings = GetContentSettings();
  std::unique_ptr<net::CanonicalCookie> first_party_cookie(
      net::CanonicalCookie::CreateForTesting(
          GURL(kThirdPartyUrl), "C=D", base::Time::Now(),
          /*server_time=*/std::nullopt,
          net::CookiePartitionKey::FromURLForTesting(GURL(kThirdPartyUrl))));
  ASSERT_TRUE(first_party_cookie);
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kThirdPartyUrl),
       GURL(kThirdPartyUrl),
       {{*first_party_cookie}},
       /* blocked_by_policy = */ false});
  content_settings->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL(kThirdPartyUrl)),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto sites = delegate->GetAllSites();
  ASSERT_EQ(sites.size(), 1u);

  auto first_site = sites[0];
  EXPECT_EQ(first_site.origin.host(), GURL(kThirdPartyUrl).host());
  EXPECT_EQ(first_site.setting, CONTENT_SETTING_ALLOW);
  // False due to first-party storage being accessed.
  EXPECT_EQ(first_site.is_fully_partitioned, false);
}

TEST_F(PageSpecificSiteDataDialogUnitTest, QuotaStorageAccessedThirdParty) {
  // Verify that storage access through BrowsingDataModel is correctly displayed
  // in the dialog.
  auto* content_settings = GetContentSettings();
  content_settings->OnBrowsingDataAccessed(
      CreateThirdPartyStorageKey(GURL(kThirdPartyUrl), GURL(kCurrentUrl)),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto sites = delegate->GetAllSites();
  ASSERT_EQ(sites.size(), 1u);

  auto first_site = sites[0];
  EXPECT_EQ(first_site.origin.host(), GURL(kThirdPartyUrl).host());
  EXPECT_EQ(first_site.setting, CONTENT_SETTING_ALLOW);
  // True due to only third-party storage being accessed.
  EXPECT_EQ(first_site.is_fully_partitioned, true);
}

TEST_F(PageSpecificSiteDataDialogUnitTest,
       QuotaStorageAndCookieAccessedThirdParty) {
  // Verify that storage access through BrowsingDataModel
  // is correctly displayed in the dialog.
  auto* content_settings = GetContentSettings();
  std::unique_ptr<net::CanonicalCookie> third_party_cookie(
      net::CanonicalCookie::CreateForTesting(GURL(kThirdPartyUrl), "C=D",
                                             base::Time::Now()));
  ASSERT_TRUE(third_party_cookie);
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kThirdPartyUrl),
       GURL(kCurrentUrl),
       {{*third_party_cookie}},
       /* blocked_by_policy = */ false});
  content_settings->OnBrowsingDataAccessed(
      CreateThirdPartyStorageKey(GURL(kThirdPartyUrl), GURL(kCurrentUrl)),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto sites = delegate->GetAllSites();
  ASSERT_EQ(sites.size(), 1u);

  auto first_site = sites[0];
  EXPECT_EQ(first_site.origin.host(), GURL(kThirdPartyUrl).host());
  EXPECT_EQ(first_site.setting, CONTENT_SETTING_ALLOW);
  // False due to cookies being accessed without forced partitioning.
  EXPECT_EQ(first_site.is_fully_partitioned, false);
}

TEST_F(PageSpecificSiteDataDialogUnitTest,
       QuotaStorageAndPartitionedCookieAccessedThirdParty) {
  // Verify that storage access through BrowsingDataModel
  // is correctly displayed in the dialog.
  auto* content_settings = GetContentSettings();
  std::unique_ptr<net::CanonicalCookie> third_party_cookie(
      net::CanonicalCookie::CreateForTesting(
          GURL(kThirdPartyUrl), "C=D", base::Time::Now(),
          /*server_time=*/std::nullopt,
          net::CookiePartitionKey::FromURLForTesting(GURL(kCurrentUrl))));
  ASSERT_TRUE(third_party_cookie);
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kThirdPartyUrl),
       GURL(kCurrentUrl),
       {{*third_party_cookie}},
       /* blocked_by_policy = */ false});
  content_settings->OnBrowsingDataAccessed(
      CreateThirdPartyStorageKey(GURL(kThirdPartyUrl), GURL(kCurrentUrl)),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto sites = delegate->GetAllSites();
  ASSERT_EQ(sites.size(), 1u);

  auto first_site = sites[0];
  EXPECT_EQ(first_site.origin.host(), GURL(kThirdPartyUrl).host());
  EXPECT_EQ(first_site.setting, CONTENT_SETTING_ALLOW);
  // TODO(crbug.com/40231917): Fix this test to return true once cookie
  // partition logic is tested.
  EXPECT_EQ(first_site.is_fully_partitioned, false);
}

TEST_F(PageSpecificSiteDataDialogUnitTest, QuotaStorageAccessedMixedParty) {
  // Verify that storage access through BrowsingDataModel is correctly displayed
  // in the dialog.
  auto* content_settings = GetContentSettings();
  content_settings->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL(kThirdPartyUrl)),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  content_settings->OnBrowsingDataAccessed(
      CreateThirdPartyStorageKey(GURL(kThirdPartyUrl), GURL(kCurrentUrl)),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto sites = delegate->GetAllSites();
  ASSERT_EQ(sites.size(), 1u);

  auto first_site = sites[0];
  EXPECT_EQ(first_site.origin.host(), GURL(kThirdPartyUrl).host());
  EXPECT_EQ(first_site.setting, CONTENT_SETTING_ALLOW);
  // False due to first-party storage being accessed.
  EXPECT_EQ(first_site.is_fully_partitioned, false);
}

TEST_F(PageSpecificSiteDataDialogUnitTest,
       QuotaStorageAndCookieAccessedMixedParty) {
  // Verify that storage access through BrowsingDataModel
  // is correctly displayed in the dialog.
  auto* content_settings = GetContentSettings();
  std::unique_ptr<net::CanonicalCookie> first_party_cookie(
      net::CanonicalCookie::CreateForTesting(GURL(kThirdPartyUrl), "C=D",
                                             base::Time::Now()));
  ASSERT_TRUE(first_party_cookie);
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kThirdPartyUrl),
       GURL(kThirdPartyUrl),
       {{*first_party_cookie}},
       /* blocked_by_policy = */ false});
  content_settings->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL(kThirdPartyUrl)),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  std::unique_ptr<net::CanonicalCookie> third_party_cookie(
      net::CanonicalCookie::CreateForTesting(GURL(kThirdPartyUrl), "C=D",
                                             base::Time::Now()));
  ASSERT_TRUE(third_party_cookie);
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kThirdPartyUrl),
       GURL(kCurrentUrl),
       {{*third_party_cookie}},
       /* blocked_by_policy = */ false});
  content_settings->OnBrowsingDataAccessed(
      CreateThirdPartyStorageKey(GURL(kThirdPartyUrl), GURL(kCurrentUrl)),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto sites = delegate->GetAllSites();
  ASSERT_EQ(sites.size(), 1u);

  auto first_site = sites[0];
  EXPECT_EQ(first_site.origin.host(), GURL(kThirdPartyUrl).host());
  EXPECT_EQ(first_site.setting, CONTENT_SETTING_ALLOW);
  // False due to first-party storage being accessed.
  EXPECT_EQ(first_site.is_fully_partitioned, false);
}

TEST_F(PageSpecificSiteDataDialogUnitTest,
       QuotaStorageAndPartitionedCookieAccessedMixedParty) {
  // Verify that storage access through BrowsingDataModel
  // is correctly displayed in the dialog.
  auto* content_settings = GetContentSettings();
  std::unique_ptr<net::CanonicalCookie> first_party_cookie(
      net::CanonicalCookie::CreateForTesting(
          GURL(kThirdPartyUrl), "C=D", base::Time::Now(),
          /*server_time=*/std::nullopt,
          net::CookiePartitionKey::FromURLForTesting(GURL(kThirdPartyUrl))));
  ASSERT_TRUE(first_party_cookie);
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kThirdPartyUrl),
       GURL(kThirdPartyUrl),
       {{*first_party_cookie}},
       /* blocked_by_policy = */ false});
  content_settings->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL(kThirdPartyUrl)),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  std::unique_ptr<net::CanonicalCookie> third_party_cookie(
      net::CanonicalCookie::CreateForTesting(
          GURL(kThirdPartyUrl), "C=D", base::Time::Now(),
          /*server_time=*/std::nullopt,
          net::CookiePartitionKey::FromURLForTesting(GURL(kCurrentUrl))));
  ASSERT_TRUE(third_party_cookie);
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kThirdPartyUrl),
       GURL(kCurrentUrl),
       {{*third_party_cookie}},
       /* blocked_by_policy = */ false});
  content_settings->OnBrowsingDataAccessed(
      CreateThirdPartyStorageKey(GURL(kThirdPartyUrl), GURL(kCurrentUrl)),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto sites = delegate->GetAllSites();
  ASSERT_EQ(sites.size(), 1u);

  auto first_site = sites[0];
  EXPECT_EQ(first_site.origin.host(), GURL(kThirdPartyUrl).host());
  EXPECT_EQ(first_site.setting, CONTENT_SETTING_ALLOW);
  // False due to first-party storage being accessed.
  EXPECT_EQ(first_site.is_fully_partitioned, false);
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

TEST_F(PageSpecificSiteDataDialogUnitTest, RemovePartitionedStorage) {
  url::Origin exampleUrlOrigin = url::Origin::Create(GURL(kExampleUrl));
  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  // Remove origins from the dialog.
  delegate->DeleteStoredObjects(exampleUrlOrigin);
  // Verify that the data was removed.
  ValidateAllowedUnpartitionedSites(delegate.get(), {});
}

TEST_F(PageSpecificSiteDataDialogUnitTest, ServiceWorkerAccessed) {
  // Verify that site data access through BrowsingDataModel is correctly
  // displayed in the dialog.
  auto* content_settings = GetContentSettings();
  auto current_url = GURL(kCurrentUrl);
  content_settings->OnServiceWorkerAccessed(
      current_url, CreateUnpartitionedStorageKey(current_url),
      content::AllowServiceWorkerResult::Yes());
  auto third_party_url = GURL(kThirdPartyUrl);
  content_settings->OnServiceWorkerAccessed(
      third_party_url, CreateUnpartitionedStorageKey(third_party_url),
      content::AllowServiceWorkerResult::Yes());

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());

  ValidateAllowedUnpartitionedSites(delegate.get(),
                                    {current_url, third_party_url});
}

TEST_F(PageSpecificSiteDataDialogUnitTest, RemoveBrowsingData) {
  // Setup a cookie for `kCurrentUrl`.
  std::unique_ptr<net::CanonicalCookie> first_party_cookie(
      net::CanonicalCookie::CreateForTesting(GURL(kCurrentUrl), "A=B",
                                             base::Time::Now()));
  ASSERT_TRUE(first_party_cookie);

  auto allowed_browsing_data_model = std::make_unique<FakeBrowsingDataModel>();
  auto blocked_browsing_data_model = std::make_unique<FakeBrowsingDataModel>();
  allowed_browsing_data_model->AddBrowsingData(
      *first_party_cookie, BrowsingDataModel::StorageType::kCookie,
      /*storage_size=*/0, /*cookie_count=*/1);

  // Setup browsing data models and populate them.
  url::Origin currentUrlOrigin = url::Origin::Create(GURL(kCurrentUrl));
  url::Origin thirdPartyUrlOrigin = url::Origin::Create(GURL(kThirdPartyUrl));
  BrowsingDataModel::DataKey thirdPartyDataKey =
      blink::StorageKey::CreateFirstParty(thirdPartyUrlOrigin);
  blocked_browsing_data_model->AddBrowsingData(
      thirdPartyDataKey, BrowsingDataModel::StorageType::kSharedStorage,
      /*storage_size=*/0);

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  delegate->SetBrowsingDataModels(allowed_browsing_data_model.get(),
                                  blocked_browsing_data_model.get());

  {
    auto sites = delegate->GetAllSites();
    std::vector<PageSpecificSiteDataDialogSite> expected_sites = {
        {currentUrlOrigin, /*settings=*/CONTENT_SETTING_ALLOW,
         /*is_fully_partitioned=*/false},
        {thirdPartyUrlOrigin, /*settings=*/CONTENT_SETTING_BLOCK,
         /*is_fully_partitioned=*/false}};

    EXPECT_THAT(sites, testing::UnorderedElementsAreArray(expected_sites));
  }

  // Remove origins from the dialog.
  delegate->DeleteStoredObjects(thirdPartyUrlOrigin);

  // Validate that sites are removed from the browsing data model.
  {
    auto sites = delegate->GetAllSites();
    std::vector<PageSpecificSiteDataDialogSite> expected_sites = {
        {currentUrlOrigin, /*settings=*/CONTENT_SETTING_ALLOW,
         /*is_fully_partitioned=*/false}};

    EXPECT_THAT(sites, testing::UnorderedElementsAreArray(expected_sites));
  }
}

class PageSpecificSiteDataDialogWithWebAppsUnitTest
    : public PageSpecificSiteDataDialogUnitTest {
 public:
  void SetUp() override {
    PageSpecificSiteDataDialogUnitTest::SetUp();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      features::kPageSpecificDataDialogRelatedInstalledAppsSection};
};

// TODO(crbug.com/362922563): Add a new test once the uninstall behavior is
// implemented.

TEST_F(PageSpecificSiteDataDialogWithWebAppsUnitTest, CorrectAppDataReturned) {
  auto app_id = web_app::test::InstallDummyWebApp(profile(), "DummyApp",
                                                  GURL(kCurrentUrl));
  auto sub_app_id = web_app::test::InstallDummyWebApp(profile(), "DummySubApp",
                                                      GURL(kCurrentUrlSubPage));
  auto unrelated_app_id = web_app::test::InstallDummyWebApp(
      profile(), "UnrelatedDummyApp", GURL(kThirdPartyUrl));

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  std::vector<webapps::AppId> app_list = delegate->GetInstalledRelatedApps();

  EXPECT_EQ(app_list.size(), 2u);
  EXPECT_THAT(app_list, Contains(app_id));
  EXPECT_THAT(app_list, Contains(sub_app_id));
  EXPECT_THAT(app_list, Not(Contains(unrelated_app_id)));
}
