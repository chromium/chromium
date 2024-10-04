// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_helper.h"

#include "components/browsing_data/content/browsing_data_model.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace browsing_data {
namespace {

class BrowsingDataHelperTest : public testing::Test {
 public:
  BrowsingDataHelperTest() = default;

  BrowsingDataHelperTest(const BrowsingDataHelperTest&) = delete;
  BrowsingDataHelperTest& operator=(const BrowsingDataHelperTest&) = delete;

  ~BrowsingDataHelperTest() override {}

  bool IsWebScheme(const std::string& scheme) {
    GURL test(scheme + "://example.com");
    return (HasWebScheme(test) && browsing_data::IsWebScheme(scheme));
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
};

TEST_F(BrowsingDataHelperTest, WebStorageSchemesAreWebSchemes) {
  EXPECT_TRUE(IsWebScheme(url::kHttpScheme));
  EXPECT_TRUE(IsWebScheme(url::kHttpsScheme));
  EXPECT_TRUE(IsWebScheme(url::kFileScheme));
  EXPECT_TRUE(IsWebScheme(url::kFtpScheme));
  EXPECT_TRUE(IsWebScheme(url::kWsScheme));
  EXPECT_TRUE(IsWebScheme(url::kWssScheme));
}

TEST_F(BrowsingDataHelperTest, ChromeSchemesAreNotWebSchemes) {
  EXPECT_FALSE(IsWebScheme(url::kAboutScheme));
  EXPECT_FALSE(IsWebScheme(content::kChromeDevToolsScheme));
  EXPECT_FALSE(IsWebScheme(content::kChromeUIScheme));
  EXPECT_FALSE(IsWebScheme(url::kJavaScriptScheme));
  EXPECT_FALSE(IsWebScheme(url::kMailToScheme));
  EXPECT_FALSE(IsWebScheme(content::kViewSourceScheme));
}

TEST_F(BrowsingDataHelperTest, SchemesThatCantStoreDataDontMatchAnything) {
  EXPECT_FALSE(IsWebScheme(url::kDataScheme));
  EXPECT_FALSE(IsWebScheme("feed"));
  EXPECT_FALSE(IsWebScheme(url::kBlobScheme));
  EXPECT_FALSE(IsWebScheme(url::kFileSystemScheme));
  EXPECT_FALSE(IsWebScheme("invalid-scheme-i-just-made-up"));
}

TEST_F(BrowsingDataHelperTest, GetUniqueThirdPartyCookiesHostCount) {
  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BrowsingDataModel::BuildEmpty(
          browser_context_.GetDefaultStoragePartition(), /*delegate=*/nullptr);

  // 1.
  auto google_url = GURL("http://google.com");
  browsing_data_model->AddBrowsingData(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(google_url)),
      BrowsingDataModel::StorageType::kLocalStorage,
      /*storage_size=*/0,
      /*cookie_count=*/0);

  // Should be consolidated in first entry.
  browsing_data_model->AddBrowsingData(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(google_url)),
      BrowsingDataModel::StorageType::kSharedStorage,
      /*storage_size=*/0,
      /*cookie_count=*/0);

  // 2. Subdomains should be treated separately.
  auto google_subdomain_url = GURL("http://maps.google.com");
  browsing_data_model->AddBrowsingData(
      blink::StorageKey::CreateFirstParty(
          url::Origin::Create(google_subdomain_url)),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*storage_size=*/0,
      /*cookie_count=*/0);

  // 3.
  auto localhost_url = GURL("http://localhost");
  browsing_data_model->AddBrowsingData(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(localhost_url)),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*storage_size=*/0,
      /*cookie_count=*/0);

  // 4.
  auto localhost_ip_url = GURL("http://127.0.0.1");
  browsing_data_model->AddBrowsingData(
      blink::StorageKey::CreateFirstParty(
          url::Origin::Create(localhost_ip_url)),
      BrowsingDataModel::StorageType::kLocalStorage,
      /*storage_size=*/0,
      /*cookie_count=*/0);

  // 5.
  auto example_url = GURL("http://example.com");
  auto example_origin = url::Origin::Create(example_url);
  // Should be counted once in the unique hosts.
  browsing_data_model->AddBrowsingData(
      example_origin, BrowsingDataModel::StorageType::kTrustTokens,
      /*storage_size=*/0,
      /*cookie_count=*/0);

  // 6.
  auto ip_url = GURL("http://192.168.1.1");
  browsing_data_model->AddBrowsingData(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(ip_url)),
      BrowsingDataModel::StorageType::kLocalStorage,
      /*storage_size=*/0,
      /*cookie_count=*/0);

  // When `google_url` is the top frame unique third-party count should sites
  // other than google URLs (out of 6 entries should be 4).
  int unique_site_count =
      GetUniqueThirdPartyCookiesHostCount(google_url, *browsing_data_model);
  EXPECT_EQ(3, unique_site_count);

  // When `google_subdomain_url` is the top frame unique third-party count
  // should sites other than google URLs (out of 6 entries should be 4).
  unique_site_count = GetUniqueThirdPartyCookiesHostCount(google_subdomain_url,
                                                          *browsing_data_model);
  EXPECT_EQ(3, unique_site_count);

  // When `ip_url` is the top frame this tests empty top frame domain with other
  // sites. Subdomains are counted separately because they're different hosts
  // (out of 6 entries should be 5).
  unique_site_count =
      GetUniqueThirdPartyCookiesHostCount(ip_url, *browsing_data_model);
  EXPECT_EQ(4, unique_site_count);
}

TEST_F(BrowsingDataHelperTest, ABAEmbedCookies) {
  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BrowsingDataModel::BuildEmpty(
          browser_context_.GetDefaultStoragePartition(), /*delegate=*/nullptr);

  // 1P cookies accessed in contexts with a cross-site ancestor (aka ABA embeds)
  // should also be counted as third-party cookies.
  browsing_data_model->AddBrowsingData(
      *net::CanonicalCookie::CreateForTesting(
          GURL("https://example.com/"), "abc=123; SameSite=None; Secure",
          base::Time::Now(),
          /*server_time=*/std::nullopt,
          /*cookie_partition_key=*/std::nullopt, net::CookieSourceType::kOther),
      BrowsingDataModel::StorageType::kCookie,
      /*storage_size=*/0,
      /*cookie_count=*/1,
      /*blocked_third_party=*/true);

  EXPECT_EQ(1, GetUniqueThirdPartyCookiesHostCount(GURL("https://example.com/"),
                                                   *browsing_data_model));
}

}  // namespace
}  // namespace browsing_data
