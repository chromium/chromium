// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/browsing_data/content/fake_browsing_data_model.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/common/content_settings_manager.mojom.h"
#include "components/page_info/core/features.h"
#include "content/public/browser/cookie_access_details.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using StorageType =
    content_settings::mojom::ContentSettingsManager::StorageType;

const char kCurrentUrl[] = "https://google.com";
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
    feature_list_.InitAndEnableFeature(page_info::kPageSpecificSiteDataDialog);
    NavigateAndCommit(GURL(kCurrentUrl));
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
            web_contents()));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
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
  ValidateAllowedUnpartitionedSites(delegate.get(),
                                    {GURL(kCurrentUrl), GURL(kThirdPartyUrl)});
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

TEST_F(PageSpecificSiteDataDialogUnitTest, RemoveOnlyBrowsingData) {
  // Setup CookieTreeModel and add cookie for `kCurrentUrl`.
  auto* content_settings = GetContentSettings();

  std::unique_ptr<net::CanonicalCookie> first_party_cookie(
      net::CanonicalCookie::Create(GURL(kCurrentUrl), "A=B", base::Time::Now(),
                                   absl::nullopt /* server_time */,
                                   absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(first_party_cookie);
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kCurrentUrl),
       GURL(kCurrentUrl),
       {*first_party_cookie},
       /*blocked_by_policy=*/false});

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto allowed_browsing_data_model = std::make_unique<FakeBrowsingDataModel>();
  auto blocked_browsing_data_model = std::make_unique<FakeBrowsingDataModel>();

  // Setup browsing data models and populate them.
  delegate->SetBrowsingDataModels(allowed_browsing_data_model.get(),
                                  blocked_browsing_data_model.get());

  url::Origin currentUrlOrigin = url::Origin::Create(GURL(kCurrentUrl));
  url::Origin thirdPartyUrlOrigin = url::Origin::Create(GURL(kThirdPartyUrl));
  BrowsingDataModel::DataKey thirdPartyDataKey =
      blink::StorageKey(thirdPartyUrlOrigin);
  blocked_browsing_data_model->AddBrowsingData(
      thirdPartyDataKey, BrowsingDataModel::StorageType::kSharedStorage,
      /*storage_size=*/0);
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

TEST_F(PageSpecificSiteDataDialogUnitTest, RemoveOnlyCookieTreeData) {
  // Setup CookieTreeModel and add cookie for `kCurrentUrl`.
  auto* content_settings = GetContentSettings();

  std::unique_ptr<net::CanonicalCookie> first_party_cookie(
      net::CanonicalCookie::Create(GURL(kCurrentUrl), "A=B", base::Time::Now(),
                                   absl::nullopt /* server_time */,
                                   absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(first_party_cookie);
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kCurrentUrl),
       GURL(kCurrentUrl),
       {*first_party_cookie},
       /*blocked_by_policy=*/false});

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto allowed_browsing_data_model = std::make_unique<FakeBrowsingDataModel>();
  auto blocked_browsing_data_model = std::make_unique<FakeBrowsingDataModel>();
  // Setup browsing data models and populate them.
  delegate->SetBrowsingDataModels(allowed_browsing_data_model.get(),
                                  blocked_browsing_data_model.get());

  url::Origin currentUrlOrigin = url::Origin::Create(GURL(kCurrentUrl));
  url::Origin thirdPartyUrlOrigin = url::Origin::Create(GURL(kThirdPartyUrl));
  BrowsingDataModel::DataKey thirdPartyDataKey =
      blink::StorageKey(thirdPartyUrlOrigin);
  blocked_browsing_data_model->AddBrowsingData(
      thirdPartyDataKey, BrowsingDataModel::StorageType::kSharedStorage,
      /*storage_size=*/0);

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
  delegate->DeleteStoredObjects(currentUrlOrigin);

  // Validate that sites are removed from the cookie tree model.
  {
    auto sites = delegate->GetAllSites();
    std::vector<PageSpecificSiteDataDialogSite> expected_sites = {
        {thirdPartyUrlOrigin, /*settings=*/CONTENT_SETTING_BLOCK,
         /*is_fully_partitioned=*/false}};

    EXPECT_THAT(sites, testing::UnorderedElementsAreArray(expected_sites));
  }
}

TEST_F(PageSpecificSiteDataDialogUnitTest, RemoveMixedModelData) {
  // Setup CookieTreeModel and add cookie for `kCurrentUrl` and `kExampleUrl`.
  auto* content_settings = GetContentSettings();

  std::unique_ptr<net::CanonicalCookie> first_party_cookie(
      net::CanonicalCookie::Create(GURL(kCurrentUrl), "A=B", base::Time::Now(),
                                   absl::nullopt /* server_time */,
                                   absl::nullopt /* cookie_partition_key */));
  std::unique_ptr<net::CanonicalCookie> example_cookie(
      net::CanonicalCookie::Create(GURL(kExampleUrl), "E=F", base::Time::Now(),
                                   absl::nullopt /* server_time */,
                                   absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(first_party_cookie);
  ASSERT_TRUE(example_cookie);
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kCurrentUrl),
       GURL(kCurrentUrl),
       {*first_party_cookie},
       /*blocked_by_policy=*/false});
  content_settings->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kRead,
       GURL(kExampleUrl),
       /*firstparty*/ GURL(kCurrentUrl),
       {*example_cookie},
       /*blocked_by_policy=*/false});

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  auto allowed_browsing_data_model = std::make_unique<FakeBrowsingDataModel>();
  auto blocked_browsing_data_model = std::make_unique<FakeBrowsingDataModel>();

  // Setup browsing data models and populate them.
  delegate->SetBrowsingDataModels(allowed_browsing_data_model.get(),
                                  blocked_browsing_data_model.get());

  url::Origin exampleUrlOrigin = url::Origin::Create(GURL(kExampleUrl));
  url::Origin currentUrlOrigin = url::Origin::Create(GURL(kCurrentUrl));
  url::Origin thirdPartyUrlOrigin = url::Origin::Create(GURL(kThirdPartyUrl));

  BrowsingDataModel::DataKey exampleDataKey =
      blink::StorageKey(exampleUrlOrigin);
  allowed_browsing_data_model->AddBrowsingData(
      exampleDataKey, BrowsingDataModel::StorageType::kSharedStorage,
      /*storage_size=*/0);
  BrowsingDataModel::DataKey thirdPartyDataKey =
      blink::StorageKey(thirdPartyUrlOrigin);
  blocked_browsing_data_model->AddBrowsingData(
      thirdPartyDataKey, BrowsingDataModel::StorageType::kSharedStorage,
      /*storage_size=*/0);

  {
    auto sites = delegate->GetAllSites();
    std::vector<PageSpecificSiteDataDialogSite> expected_sites = {
        {exampleUrlOrigin, /*settings=*/CONTENT_SETTING_ALLOW,
         /*is_fully_partitioned=*/false},
        {currentUrlOrigin, /*settings=*/CONTENT_SETTING_ALLOW,
         /*is_fully_partitioned=*/false},
        {thirdPartyUrlOrigin, /*settings=*/CONTENT_SETTING_BLOCK,
         /*is_fully_partitioned=*/false}};

    EXPECT_THAT(sites, testing::UnorderedElementsAreArray(expected_sites));
  }

  // Remove origins from the dialog.
  delegate->DeleteStoredObjects(exampleUrlOrigin);

  // Validate that sites are removed from both models.
  {
    auto sites = delegate->GetAllSites();
    std::vector<PageSpecificSiteDataDialogSite> expected_sites = {
        {currentUrlOrigin, /*settings=*/CONTENT_SETTING_ALLOW,
         /*is_fully_partitioned=*/false},
        {thirdPartyUrlOrigin, /*settings=*/CONTENT_SETTING_BLOCK,
         /*is_fully_partitioned=*/false}};

    EXPECT_THAT(sites, testing::UnorderedElementsAreArray(expected_sites));
  }
}

TEST_F(PageSpecificSiteDataDialogUnitTest, ServiceWorkerAccessed) {
  // Verify that site data access through CookiesTreeModel is correctly
  // displayed in the dialog.
  auto* content_settings = GetContentSettings();

  content_settings->OnServiceWorkerAccessed(
      GURL(kCurrentUrl), content::AllowServiceWorkerResult::Yes());
  content_settings->OnServiceWorkerAccessed(
      GURL(kThirdPartyUrl), content::AllowServiceWorkerResult::Yes());

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());

  ValidateAllowedUnpartitionedSites(delegate.get(),
                                    {GURL(kCurrentUrl), GURL(kThirdPartyUrl)});
}

class PageSpecificSiteDataDialogStorageUnitTest
    : public PageSpecificSiteDataDialogUnitTest,
      public testing::WithParamInterface<StorageType> {};

TEST_P(PageSpecificSiteDataDialogStorageUnitTest, StorageAccessed) {
  // Verify that storage access through CookiesTreeModel is correctly
  // displayed in the dialog.
  auto* content_settings = GetContentSettings();

  content_settings->OnStorageAccessed(GetParam(), GURL(kCurrentUrl),
                                      /*blocked_by_policy=*/false);
  content_settings->OnStorageAccessed(GetParam(), GURL(kThirdPartyUrl),
                                      /*blocked_by_policy=*/false);

  auto delegate =
      std::make_unique<test::PageSpecificSiteDataDialogTestApi>(web_contents());
  ValidateAllowedUnpartitionedSites(delegate.get(),
                                    {GURL(kCurrentUrl), GURL(kThirdPartyUrl)});
}

INSTANTIATE_TEST_SUITE_P(PageSpecificSiteDataDialogStorageUnitTestInstance,
                         PageSpecificSiteDataDialogStorageUnitTest,
                         testing::Values(StorageType::DATABASE,
                                         StorageType::LOCAL_STORAGE,
                                         StorageType::SESSION_STORAGE,
                                         StorageType::FILE_SYSTEM,
                                         StorageType::INDEXED_DB,
                                         StorageType::CACHE));
