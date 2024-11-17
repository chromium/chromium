// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/page_specific_content_settings.h"

#include <optional>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/features.h"
#include "components/security_state/core/security_state.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/trust_token_access_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_options.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"
#include "services/network/public/mojom/shared_dictionary_access_observer.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content_settings {

using StorageType = mojom::ContentSettingsManager::StorageType;

namespace {

constexpr int kTopicsAPITestTaxonomyVersion = 1;

class MockSiteDataObserver
    : public PageSpecificContentSettings::SiteDataObserver {
 public:
  explicit MockSiteDataObserver(content::WebContents* web_contents)
      : SiteDataObserver(web_contents) {}

  MockSiteDataObserver(const MockSiteDataObserver&) = delete;
  MockSiteDataObserver& operator=(const MockSiteDataObserver&) = delete;

  ~MockSiteDataObserver() override = default;

  MOCK_METHOD(void, OnSiteDataAccessed, (const AccessDetails& access_details));
  MOCK_METHOD(void, OnStatefulBounceDetected, ());
};

class MockPageSpecificContentSettingsDelegate
    : public TestPageSpecificContentSettingsDelegate {
 public:
  MockPageSpecificContentSettingsDelegate(PrefService* prefs,
                                          HostContentSettingsMap* settings_map)
      : TestPageSpecificContentSettingsDelegate(prefs, settings_map) {}
  ~MockPageSpecificContentSettingsDelegate() override = default;

  MOCK_METHOD(void, UpdateLocationBar, ());
  MOCK_METHOD(void, OnContentAllowed, (ContentSettingsType type));
  MOCK_METHOD(void, OnContentBlocked, (ContentSettingsType type));
  MOCK_METHOD(bool,
              IsFrameAllowlistedForJavaScript,
              (content::RenderFrameHost * render_frame_host));
};

blink::StorageKey CreateUnpartitionedStorageKey(const GURL& url) {
  return blink::StorageKey::CreateFirstParty(url::Origin::Create(url));
}

}  // namespace

class PageSpecificContentSettingsTest
    : public content::RenderViewHostTestHarness {
 public:
  PageSpecificContentSettingsTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session*/, false /* should_record_metrics */);
    PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<TestPageSpecificContentSettingsDelegate>(
            &prefs_, settings_map_.get()));
  }

  void TearDown() override {
    settings_map_->ShutdownOnUIThread();
    RenderViewHostTestHarness::TearDown();
  }

  HostContentSettingsMap* settings_map() { return settings_map_.get(); }
  PrefService* prefs() { return &prefs_; }

  content::WebContentsObserver* GetHandle() {
    return PageSpecificContentSettings::GetWebContentsObserverForTest(
        web_contents());
  }

  MockPageSpecificContentSettingsDelegate* InstallMockDelegate() {
    PageSpecificContentSettings::DeleteForWebContentsForTest(web_contents());
    PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<
            testing::NiceMock<MockPageSpecificContentSettingsDelegate>>(
            prefs(), settings_map()));
    return static_cast<MockPageSpecificContentSettingsDelegate*>(
        PageSpecificContentSettings::GetDelegateForWebContents(web_contents()));
  }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
};

TEST_F(PageSpecificContentSettingsTest, BlockedContent) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  // Check that after initializing, nothing is blocked.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::IMAGES));
#endif
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::SOUND));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));

  // Set a cookie, block access to images, block mediastream access and block a
  // popup.
  GURL origin("http://google.com");
  std::unique_ptr<net::CanonicalCookie> cookie1(
      net::CanonicalCookie::CreateForTesting(origin, "A=B", base::Time::Now()));
  ASSERT_TRUE(cookie1);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {{*cookie1}},
                                  false});
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  content_settings->OnContentBlocked(ContentSettingsType::IMAGES);
#endif
  content_settings->OnContentBlocked(ContentSettingsType::POPUPS);
  PageSpecificContentSettings::MicrophoneCameraState
      blocked_microphone_camera_state = {
          PageSpecificContentSettings::kMicrophoneAccessed,
          PageSpecificContentSettings::kMicrophoneBlocked,
          PageSpecificContentSettings::kCameraAccessed,
          PageSpecificContentSettings::kCameraBlocked};
  content_settings->OnMediaStreamPermissionSet(GURL("http://google.com"),
                                               blocked_microphone_camera_state);

  // Check that only the respective content types are affected.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::IMAGES));
#endif
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::SOUND));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));
  EXPECT_TRUE(
      content_settings->IsContentBlocked(ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(content_settings->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {{*cookie1}},
                                  false});

  // Block a cookie.
  std::unique_ptr<net::CanonicalCookie> cookie2(
      net::CanonicalCookie::CreateForTesting(origin, "C=D", base::Time::Now()));
  ASSERT_TRUE(cookie2);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {{*cookie2}},
                                  true});
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // Block a javascript during a navigation.
  // Create a pending navigation.
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("http://google.com"), web_contents());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();
  GetHandle()->OnServiceWorkerAccessed(
      simulator->GetNavigationHandle(), GURL("http://google.com"),
      content::AllowServiceWorkerResult::FromPolicy(true, false));
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  simulator->Commit();
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  // Block a javascript when page starts to start ServiceWorker.
  GetHandle()->OnServiceWorkerAccessed(
      web_contents()->GetPrimaryMainFrame(), GURL("http://google.com"),
      content::AllowServiceWorkerResult::FromPolicy(true, false));
  EXPECT_TRUE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));

  // Reset blocked content settings.
  NavigateAndCommit(GURL("http://google.com"));
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::IMAGES));
#endif
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
}

TEST_F(PageSpecificContentSettingsTest, BlockedFileSystems) {
  NavigateAndCommit(GURL("http://google.com"));
  auto* rfh = web_contents()->GetPrimaryMainFrame();
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(rfh);
  auto google_storage_key = rfh->GetStorageKey();
  // Access a file system.
  PageSpecificContentSettings::StorageAccessed(
      StorageType::FILE_SYSTEM, rfh->GetGlobalId(), google_storage_key, false);
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // Block access to a file system.
  PageSpecificContentSettings::StorageAccessed(
      StorageType::FILE_SYSTEM, rfh->GetGlobalId(), google_storage_key, true);
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
}

TEST_F(PageSpecificContentSettingsTest, AllowedContent) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  // Test default settings.
  ASSERT_FALSE(content_settings->IsContentAllowed(ContentSettingsType::IMAGES));
  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::MEDIASTREAM_MIC));
  ASSERT_FALSE(content_settings->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));

  // Record a cookie.
  GURL origin("http://google.com");
  std::unique_ptr<net::CanonicalCookie> cookie1(
      net::CanonicalCookie::CreateForTesting(origin, "A=B", base::Time::Now()));
  ASSERT_TRUE(cookie1);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {{*cookie1}},
                                  false});
  ASSERT_TRUE(content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // Record a blocked cookie.
  std::unique_ptr<net::CanonicalCookie> cookie2(
      net::CanonicalCookie::CreateForTesting(origin, "C=D", base::Time::Now()));
  ASSERT_TRUE(cookie2);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {{*cookie2}},
                                  true});
  ASSERT_TRUE(content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
}

TEST_F(PageSpecificContentSettingsTest, AllowlistJavaScript) {
  const GURL url("http://google.com");
  MockPageSpecificContentSettingsDelegate* mock_delegate =
      InstallMockDelegate();

  // PageSpecificContentSettingsDelegate::IsFrameAllowlistedForJavaScript() is
  // called once per navigation.
  EXPECT_CALL(*mock_delegate, IsFrameAllowlistedForJavaScript(::testing::_))
      .Times(2)
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(true));

  content::NavigationHandleObserver observer(web_contents(), url);

  // Disable JavaScript. The secondary URL is ignored. This call is functionally
  // equivalent to setting `secondary_url` = wildcard.
  settings_map()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_BLOCK);

  NavigateAndCommit(url);

  // The first mock call returns false for allowing JavaScript. The first
  // navigation should not allow JavaScript in the main frame.
  EXPECT_FALSE(observer.content_settings()->allow_script);

  // The second mock call returns true for allowing JavaScript. The second
  // navigation should allow JavaScript in the main frame.
  NavigateAndCommit(url);

  EXPECT_TRUE(observer.content_settings()->allow_script);
}

TEST_F(PageSpecificContentSettingsTest, InterestGroupJoin) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // Check that a blocking an interest join does not result in a blocked cookie
  // report.
  auto api_origin = url::Origin::Create(GURL("https://embedded.com"));
  content_settings->OnInterestGroupJoined(api_origin,
                                          /*blocked_by_policy=*/true);

  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // But that a successful join results in cookie access.
  content_settings->OnInterestGroupJoined(api_origin,
                                          /*blocked_by_policy=*/false);
  ASSERT_TRUE(content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
}

TEST_F(PageSpecificContentSettingsTest, BrowsingDataAccessed) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // Only some browsing data types should be reported as blocked cookies.
  auto origin = url::Origin::Create(GURL("https://embedded.com"));
  content_settings->OnBrowsingDataAccessed(
      origin, BrowsingDataModel::StorageType::kTrustTokens,
      /*blocked=*/true);

  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  auto storage_key = blink::StorageKey::CreateFirstParty(origin);
  content_settings->OnBrowsingDataAccessed(
      storage_key, BrowsingDataModel::StorageType::kLocalStorage,
      /*blocked=*/true);

  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // But allowed accesses should be reported.
  content_settings->OnBrowsingDataAccessed(
      origin, BrowsingDataModel::StorageType::kTrustTokens,
      /*blocked=*/false);
  ASSERT_TRUE(content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
}

TEST_F(PageSpecificContentSettingsTest, EmptyCookieList) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  GetHandle()->OnCookiesAccessed(
      web_contents()->GetPrimaryMainFrame(),
      {content::CookieAccessDetails::Type::kRead, GURL("http://google.com"),
       GURL("http://google.com"), net::CookieAccessResultList(), true});
  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
}

TEST_F(PageSpecificContentSettingsTest, BlockedThirdPartyCookie) {
  NavigateAndCommit(GURL("https://google.com"));
  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  std::unique_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::CreateForTesting(
          GURL("https://google.com"),
          "CookieName=CookieValue;Secure;SameSite=None", base::Time::Now()));

  // 1P cookie should not be blocked.
  GetHandle()->OnCookiesAccessed(
      web_contents()->GetPrimaryMainFrame(),
      {content::CookieAccessDetails::Type::kRead,
       /*url=*/GURL("https://google.com"),
       /*first_party_url=*/GURL("https://google.com"),
       {{*cookie}},
       /*blocked_by_policy=*/true,
       /*is_ad_tagged=*/false,
       net::CookieSettingOverrides(),
       net::SiteForCookies::FromUrl(GURL("https://google.com"))});

  auto* blocked_data_model = pscs->blocked_browsing_data_model();
  size_t count = 0u;
  for (const auto& entry : *blocked_data_model) {
    if (entry.data_details->blocked_third_party) {
      ++count;
    }
  }
  EXPECT_EQ(0u, count);

  // 1P cookie in ABA embed should be blocked.
  GetHandle()->OnCookiesAccessed(
      web_contents()->GetPrimaryMainFrame(),
      {content::CookieAccessDetails::Type::kRead,
       /*url=*/GURL("https://google.com"),
       /*first_party_url=*/GURL("https://google.com"),
       {{*cookie}},
       /*blocked_by_policy=*/true,
       /*is_ad_tagged=*/false,
       net::CookieSettingOverrides(),
       net::SiteForCookies()});

  count = 0u;
  for (const auto& entry : *blocked_data_model) {
    if (entry.data_details->blocked_third_party) {
      ++count;
    }
  }
  EXPECT_EQ(1u, count);

  std::unique_ptr<net::CanonicalCookie> third_party_cookie(
      net::CanonicalCookie::CreateForTesting(
          GURL("https://example.com"),
          "CookieName=CookieValue;Secure;SameSite=None", base::Time::Now()));

  // 3P cookie should be blocked.
  GetHandle()->OnCookiesAccessed(
      web_contents()->GetPrimaryMainFrame(),
      {content::CookieAccessDetails::Type::kRead,
       /*url=*/GURL("https://google.com"),
       /*first_party_url=*/GURL("https://example.com"),
       {{*third_party_cookie}},
       /*blocked_by_policy=*/true,
       /*is_ad_tagged=*/false,
       net::CookieSettingOverrides(),
       net::SiteForCookies::FromUrl(GURL("https://example.com"))});

  count = 0u;
  for (const auto& entry : *blocked_data_model) {
    if (entry.data_details->blocked_third_party) {
      ++count;
    }
  }
  EXPECT_EQ(2u, count);
}

TEST_F(PageSpecificContentSettingsTest, SiteDataObserver) {
  NavigateAndCommit(GURL("http://google.com"));
  auto* rfh = web_contents()->GetPrimaryMainFrame();
  MockSiteDataObserver mock_observer(web_contents());
  EXPECT_CALL(mock_observer, OnSiteDataAccessed).Times(6);

  bool blocked_by_policy = false;
  GURL origin("http://google.com");
  std::unique_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::CreateForTesting(origin, "A=B", base::Time::Now()));
  ASSERT_TRUE(cookie);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {{*cookie}},
                                  blocked_by_policy});

  net::CookieAccessResultList cookie_list;
  std::unique_ptr<net::CanonicalCookie> other_cookie(
      net::CanonicalCookie::CreateForTesting(GURL("http://google.com"),
                                             "CookieName=CookieValue",
                                             base::Time::Now()));
  ASSERT_TRUE(other_cookie);

  cookie_list.emplace_back(*other_cookie);
  GetHandle()->OnCookiesAccessed(
      rfh,
      {content::CookieAccessDetails::Type::kRead, GURL("http://google.com"),
       GURL("http://google.com"), cookie_list, blocked_by_policy});

  auto google_storage_key = rfh->GetStorageKey();
  PageSpecificContentSettings::StorageAccessed(
      StorageType::FILE_SYSTEM, rfh->GetGlobalId(), google_storage_key,
      blocked_by_policy);
  PageSpecificContentSettings::StorageAccessed(
      StorageType::INDEXED_DB, rfh->GetGlobalId(), google_storage_key,
      blocked_by_policy);
  PageSpecificContentSettings::StorageAccessed(
      StorageType::LOCAL_STORAGE, rfh->GetGlobalId(), google_storage_key,
      blocked_by_policy);
  PageSpecificContentSettings::StorageAccessed(
      StorageType::DATABASE, rfh->GetGlobalId(), google_storage_key,
      blocked_by_policy);
}

TEST_F(PageSpecificContentSettingsTest, BrowsingDataModelTrustToken) {
  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  auto* allowed_browsing_data_model = pscs->allowed_browsing_data_model();

  // Before Trust Token accesses, there should be no objects here.
  EXPECT_EQ(0, browsing_data::GetUniqueHostCount(*allowed_browsing_data_model));
  const url::Origin origin = url::Origin::Create(GURL("http://google.com/"));
  const url::Origin issuer =
      url::Origin::Create(GURL("http://issuer.example/"));
  // Access a Trust Token.
  GetHandle()->OnTrustTokensAccessed(
      web_contents()->GetPrimaryMainFrame(),
      content::TrustTokenAccessDetails(
          origin, network::mojom::TrustTokenOperationType::kIssuance, issuer,
          false));

  EXPECT_EQ(1, browsing_data::GetUniqueHostCount(*allowed_browsing_data_model));
}

TEST_F(PageSpecificContentSettingsTest,
       BrowsingDataModelTrustTokenPendingNavigation) {
  NavigateAndCommit(GURL("http://google.com"));

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("http://other.com"), web_contents());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();

  const url::Origin origin = url::Origin::Create(GURL("http://google.com/"));
  const url::Origin issuer =
      url::Origin::Create(GURL("http://issuer.example/"));
  // Access a Trust Token.
  GetHandle()->OnTrustTokensAccessed(
      simulator->GetNavigationHandle(),
      content::TrustTokenAccessDetails(
          origin, network::mojom::TrustTokenOperationType::kIssuance, issuer,
          false));
  simulator->Commit();

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      simulator->GetFinalRenderFrameHost());
  auto* allowed_browsing_data_model = pscs->allowed_browsing_data_model();

  // Before Trust Token accesses, there should be no objects here.
  EXPECT_EQ(1, browsing_data::GetUniqueHostCount(*allowed_browsing_data_model));
}

TEST_F(PageSpecificContentSettingsTest, BrowsingDataModelSharedDictionary) {
  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  auto* allowed_browsing_data_model = pscs->allowed_browsing_data_model();
  auto* blocked_browsing_data_model = pscs->blocked_browsing_data_model();

  // Before Shared Dictionary accesses, there should be no objects here.
  EXPECT_EQ(0, browsing_data::GetUniqueHostCount(*allowed_browsing_data_model));
  EXPECT_EQ(0, browsing_data::GetUniqueHostCount(*blocked_browsing_data_model));
  const url::Origin origin = url::Origin::Create(GURL("http://google.com/"));
  net::SharedDictionaryIsolationKey isolation_key(origin,
                                                  net::SchemefulSite(origin));
  // Access a Shared Dictionary.
  network::mojom::SharedDictionaryAccessDetailsPtr details =
      network::mojom::SharedDictionaryAccessDetails::New(
          network::mojom::SharedDictionaryAccessDetails::Type::kRead,
          GURL("http://test.example/target"), isolation_key,
          /*is_blocked=*/false);
  GetHandle()->OnSharedDictionaryAccessed(web_contents()->GetPrimaryMainFrame(),
                                          *details);

  EXPECT_EQ(1, browsing_data::GetUniqueHostCount(*allowed_browsing_data_model));
  EXPECT_EQ(0, browsing_data::GetUniqueHostCount(*blocked_browsing_data_model));
  ASSERT_EQ(1u, allowed_browsing_data_model->size());
  EXPECT_EQ("google.com",
            *absl::get_if<std::string>(
                &*(*allowed_browsing_data_model->begin()).data_owner));
}

TEST_F(PageSpecificContentSettingsTest,
       BrowsingDataModelSharedDictionaryBlocked) {
  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  auto* allowed_browsing_data_model = pscs->allowed_browsing_data_model();
  auto* blocked_browsing_data_model = pscs->blocked_browsing_data_model();

  // Before Shared Dictionary accesses, there should be no objects here.
  EXPECT_EQ(0, browsing_data::GetUniqueHostCount(*allowed_browsing_data_model));
  EXPECT_EQ(0, browsing_data::GetUniqueHostCount(*blocked_browsing_data_model));
  const url::Origin origin = url::Origin::Create(GURL("http://google.com/"));
  net::SharedDictionaryIsolationKey isolation_key(origin,
                                                  net::SchemefulSite(origin));
  // Blocked a Shared Dictionary access.
  network::mojom::SharedDictionaryAccessDetailsPtr details =
      network::mojom::SharedDictionaryAccessDetails::New(
          network::mojom::SharedDictionaryAccessDetails::Type::kRead,
          GURL("http://test.example/target"), isolation_key,
          /*is_blocked=*/true);
  GetHandle()->OnSharedDictionaryAccessed(web_contents()->GetPrimaryMainFrame(),
                                          *details);

  EXPECT_EQ(0, browsing_data::GetUniqueHostCount(*allowed_browsing_data_model));
  EXPECT_EQ(1, browsing_data::GetUniqueHostCount(*blocked_browsing_data_model));
  ASSERT_EQ(1u, blocked_browsing_data_model->size());
  EXPECT_EQ("google.com",
            *absl::get_if<std::string>(
                &*(*blocked_browsing_data_model->begin()).data_owner));
}

TEST_F(PageSpecificContentSettingsTest,
       BrowsingDataModelSharedDictionaryPendingNavigation) {
  NavigateAndCommit(GURL("http://google.com"));

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("http://other.com"), web_contents());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();

  // Access a Shared Dictionary.
  const url::Origin origin = url::Origin::Create(GURL("http://google.com/"));
  net::SharedDictionaryIsolationKey isolation_key(origin,
                                                  net::SchemefulSite(origin));
  network::mojom::SharedDictionaryAccessDetailsPtr details =
      network::mojom::SharedDictionaryAccessDetails::New(
          network::mojom::SharedDictionaryAccessDetails::Type::kRead,
          GURL("http://test.example/target"), isolation_key,
          /*is_blocked=*/false);
  GetHandle()->OnSharedDictionaryAccessed(simulator->GetNavigationHandle(),
                                          *details);
  simulator->Commit();

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      simulator->GetFinalRenderFrameHost());
  auto* allowed_browsing_data_model = pscs->allowed_browsing_data_model();
  auto* blocked_browsing_data_model = pscs->blocked_browsing_data_model();

  EXPECT_EQ(1, browsing_data::GetUniqueHostCount(*allowed_browsing_data_model));
  EXPECT_EQ(0, browsing_data::GetUniqueHostCount(*blocked_browsing_data_model));
  ASSERT_EQ(1u, allowed_browsing_data_model->size());
  EXPECT_EQ("google.com",
            *absl::get_if<std::string>(
                &*(*allowed_browsing_data_model->begin()).data_owner));
}

TEST_F(PageSpecificContentSettingsTest,
       BrowsingDataModelSharedDictionaryPendingNavigationBlocked) {
  NavigateAndCommit(GURL("http://google.com"));

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("http://other.com"), web_contents());
  simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  simulator->Start();

  // Blocked a Shared Dictionary access.
  const url::Origin origin = url::Origin::Create(GURL("http://google.com/"));
  net::SharedDictionaryIsolationKey isolation_key(origin,
                                                  net::SchemefulSite(origin));
  network::mojom::SharedDictionaryAccessDetailsPtr details =
      network::mojom::SharedDictionaryAccessDetails::New(
          network::mojom::SharedDictionaryAccessDetails::Type::kRead,
          GURL("http://test.example/target"), isolation_key,
          /*is_blocked=*/true);
  GetHandle()->OnSharedDictionaryAccessed(simulator->GetNavigationHandle(),
                                          *details);
  simulator->Commit();

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      simulator->GetFinalRenderFrameHost());
  auto* allowed_browsing_data_model = pscs->allowed_browsing_data_model();
  auto* blocked_browsing_data_model = pscs->blocked_browsing_data_model();

  EXPECT_EQ(0, browsing_data::GetUniqueHostCount(*allowed_browsing_data_model));
  EXPECT_EQ(1, browsing_data::GetUniqueHostCount(*blocked_browsing_data_model));
  ASSERT_EQ(1u, blocked_browsing_data_model->size());
  EXPECT_EQ("google.com",
            *absl::get_if<std::string>(
                &*(*blocked_browsing_data_model->begin()).data_owner));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(PageSpecificContentSettingsTest,
       IndicatorChangedOnContentSettingChange) {
  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  // First trigger OnContentBlocked.
  EXPECT_FALSE(content_settings->IsContentBlocked(
      ContentSettingsType::CLIPBOARD_READ_WRITE));
  content_settings->OnContentBlocked(ContentSettingsType::CLIPBOARD_READ_WRITE);
  EXPECT_TRUE(content_settings->IsContentBlocked(
      ContentSettingsType::CLIPBOARD_READ_WRITE));

  // Simulate the user modifying the setting.
  HostContentSettingsMap* map = settings_map();

  ContentSettingsPattern pattern =
      ContentSettingsPattern::FromURL(web_contents()->GetVisibleURL());

  map->SetWebsiteSettingCustomScope(pattern, ContentSettingsPattern::Wildcard(),
                                    ContentSettingsType::CLIPBOARD_READ_WRITE,
                                    base::Value(CONTENT_SETTING_ALLOW));

  // Now the indicator is set to allowed.
  EXPECT_TRUE(content_settings->IsContentAllowed(
      ContentSettingsType::CLIPBOARD_READ_WRITE));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      ContentSettingsType::CLIPBOARD_READ_WRITE));

  // Simulate the user modifying the setting back to blocked.
  map->SetWebsiteSettingCustomScope(pattern, ContentSettingsPattern::Wildcard(),
                                    ContentSettingsType::CLIPBOARD_READ_WRITE,
                                    base::Value(CONTENT_SETTING_BLOCK));

  // Now the indicator is set to allowed.
  EXPECT_TRUE(content_settings->IsContentBlocked(
      ContentSettingsType::CLIPBOARD_READ_WRITE));
  EXPECT_FALSE(content_settings->IsContentAllowed(
      ContentSettingsType::CLIPBOARD_READ_WRITE));
}
#endif

TEST_F(PageSpecificContentSettingsTest, AllowedSitesCountedFromBothModels) {
  // Populate containers with hosts.
  bool blocked_by_policy = false;
  auto googleURL = GURL("http://google.com");
  auto exampleURL = GURL("https://example.com");
  auto cookie1 = net::CanonicalCookie::CreateForTesting(googleURL, "k1=v",
                                                        base::Time::Now());
  auto cookie2 = net::CanonicalCookie::CreateForTesting(exampleURL, "k2=v",
                                                        base::Time::Now());
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kRead,
                                  googleURL,
                                  googleURL,
                                  {{*cookie1}},
                                  blocked_by_policy});
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kRead,
                                  exampleURL,
                                  exampleURL,
                                  {{*cookie2}},
                                  blocked_by_policy});

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  auto* allowed_browsing_data_model = pscs->allowed_browsing_data_model();
  allowed_browsing_data_model->AddBrowsingData(
      url::Origin::Create(exampleURL),
      BrowsingDataModel::StorageType::kTrustTokens, /*storage_size=*/0);

  // Verify the size is counted without duplication of hosts.
  EXPECT_EQ(2, browsing_data::GetUniqueHostCount(*allowed_browsing_data_model));
}

TEST_F(PageSpecificContentSettingsTest, BrowsingDataModelStorageAccess) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  bool blocked_by_policy = false;

  content_settings->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://www.google.com")),
      BrowsingDataModel::StorageType::kQuotaStorage, blocked_by_policy);
  content_settings->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("http://example.com")),
      BrowsingDataModel::StorageType::kLocalStorage, blocked_by_policy);
  content_settings->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://www.youtube.com")),
      BrowsingDataModel::StorageType::kSharedStorage, blocked_by_policy);

  const auto* allowed_browsing_data_model =
      content_settings->allowed_browsing_data_model();

  EXPECT_EQ(3, browsing_data::GetUniqueHostCount(*allowed_browsing_data_model));
}

class PageSpecificContentSettingsWithPrerenderTest
    : public PageSpecificContentSettingsTest {
 public:
  PageSpecificContentSettingsWithPrerenderTest() = default;

  content::RenderFrameHost* AddPrerender(const GURL& prerender_url) {
    web_contents_delegate_ =
        std::make_unique<content::test::ScopedPrerenderWebContentsDelegate>(
            *web_contents());
    content::RenderFrameHost* prerender_frame =
        content::WebContentsTester::For(web_contents())
            ->AddPrerenderAndCommitNavigation(prerender_url);
    DCHECK(prerender_frame);
    DCHECK_EQ(prerender_frame->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kPrerendering);
    DCHECK_EQ(prerender_frame->GetLastCommittedURL(), prerender_url);
    return prerender_frame;
  }

 private:
  content::test::ScopedPrerenderFeatureList prerender_feature_list_;
  std::unique_ptr<content::test::ScopedPrerenderWebContentsDelegate>
      web_contents_delegate_;
};

TEST_F(PageSpecificContentSettingsWithPrerenderTest, SiteDataAccessed) {
  NavigateAndCommit(GURL("http://google.com"));
  const GURL& prerender_url = GURL("http://google.com/foo");
  content::RenderFrameHost* prerender_frame = AddPrerender(prerender_url);
  PageSpecificContentSettings* pscs =
      PageSpecificContentSettings::GetForFrame(prerender_frame);
  ASSERT_NE(pscs, nullptr);

  // Simulate cookie access.
  {
    MockSiteDataObserver mock_observer(web_contents());
    // OnSiteDataAccessed should not be called for prerendering page.
    EXPECT_CALL(mock_observer, OnSiteDataAccessed).Times(0);
    // Set a cookie, block access to images, block mediastream access and block
    // a popup.
    GURL origin("http://google.com");
    std::unique_ptr<net::CanonicalCookie> cookie1(
        net::CanonicalCookie::CreateForTesting(origin, "A=B",
                                               base::Time::Now()));
    ASSERT_TRUE(cookie1);
    pscs->OnCookiesAccessed({content::CookieAccessDetails::Type::kChange,
                             origin,
                             origin,
                             {{*cookie1}},
                             false});
  }
  // Activate prerendering page.
  {
    MockSiteDataObserver mock_observer(web_contents());
    // OnSiteDataAccessed should be called after page is activated.
    EXPECT_CALL(mock_observer, OnSiteDataAccessed).Times(1);
    std::unique_ptr<content::NavigationSimulator> navigation =
        content::NavigationSimulator::CreateRendererInitiated(
            prerender_url, web_contents()->GetPrimaryMainFrame());
    // TODO(crbug.com/40170513): Investigate how default referrer value
    // is set and update here accordingly.
    navigation->SetReferrer(blink::mojom::Referrer::New(
        web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
        network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
    navigation->Commit();
  }
}

TEST_F(PageSpecificContentSettingsWithPrerenderTest,
       DelegateUpdatesSentAfterActivation) {
  MockPageSpecificContentSettingsDelegate* mock_delegate =
      InstallMockDelegate();
  NavigateAndCommit(GURL("http://google.com"));
  const GURL& prerender_url = GURL("http://google.com/foo");
  content::RenderFrameHost* prerender_frame = AddPrerender(prerender_url);
  PageSpecificContentSettings* pscs =
      PageSpecificContentSettings::GetForFrame(prerender_frame);
  ASSERT_NE(pscs, nullptr);

  EXPECT_CALL(*mock_delegate, OnContentAllowed).Times(0);
  EXPECT_CALL(*mock_delegate, OnContentBlocked).Times(0);

  const GURL url = GURL("http://google.com");
  auto cookie =
      net::CanonicalCookie::CreateForTesting(url, "k=v", base::Time::Now());
  pscs->OnCookiesAccessed({content::CookieAccessDetails::Type::kRead,
                           url,
                           url,
                           {{*cookie}},
                           /*blocked_by_policy=*/false});
  PageSpecificContentSettings::StorageAccessed(StorageType::INDEXED_DB,
                                               prerender_frame->GetGlobalId(),
                                               prerender_frame->GetStorageKey(),
                                               /*blocked_by_policy=*/true);

  EXPECT_CALL(*mock_delegate, OnContentAllowed(ContentSettingsType::COOKIES))
      .Times(1);
  EXPECT_CALL(*mock_delegate, OnContentBlocked(ContentSettingsType::COOKIES))
      .Times(1);
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(
          prerender_url, web_contents()->GetPrimaryMainFrame());
  navigation->SetReferrer(blink::mojom::Referrer::New(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
      network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
  navigation->Commit();
}

TEST_F(PageSpecificContentSettingsWithPrerenderTest,
       UpdateLocationBarAfterActivation) {
  MockPageSpecificContentSettingsDelegate* mock_delegate =
      InstallMockDelegate();
  NavigateAndCommit(GURL("http://google.com"));
  const GURL& prerender_url = GURL("http://google.com/foo");
  content::RenderFrameHost* prerender_frame = AddPrerender(prerender_url);
  PageSpecificContentSettings* pscs =
      PageSpecificContentSettings::GetForFrame(prerender_frame);
  ASSERT_NE(pscs, nullptr);

  EXPECT_CALL(*mock_delegate, UpdateLocationBar()).Times(0);
  pscs->OnContentBlocked(ContentSettingsType::JAVASCRIPT);

  EXPECT_CALL(*mock_delegate, UpdateLocationBar()).Times(1);
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(
          prerender_url, web_contents()->GetPrimaryMainFrame());
  navigation->Commit();
}

TEST_F(PageSpecificContentSettingsWithPrerenderTest, ContentAllowedAndBlocked) {
  MockPageSpecificContentSettingsDelegate* mock_delegate =
      InstallMockDelegate();
  NavigateAndCommit(GURL("http://google.com"));
  const GURL& prerender_url = GURL("http://google.com/foo");
  content::RenderFrameHost* prerender_frame = AddPrerender(prerender_url);
  PageSpecificContentSettings* pscs =
      PageSpecificContentSettings::GetForFrame(prerender_frame);
  ASSERT_NE(pscs, nullptr);

  EXPECT_CALL(*mock_delegate, OnContentAllowed).Times(0);
  EXPECT_CALL(*mock_delegate, OnContentBlocked).Times(0);
  pscs->OnContentBlocked(ContentSettingsType::JAVASCRIPT);
  pscs->OnContentAllowed(ContentSettingsType::COOKIES);

  EXPECT_CALL(*mock_delegate, OnContentBlocked(ContentSettingsType::JAVASCRIPT))
      .Times(1);
  EXPECT_CALL(*mock_delegate, OnContentAllowed(ContentSettingsType::COOKIES))
      .Times(1);
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(
          prerender_url, web_contents()->GetPrimaryMainFrame());
  // TODO(crbug.com/40170513): Investigate how default referrer value is
  // set and update here accordingly.
  navigation->SetReferrer(blink::mojom::Referrer::New(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
      network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
  navigation->Commit();
}

TEST_F(PageSpecificContentSettingsTest, Topics) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  EXPECT_FALSE(pscs->HasAccessedTopics());
  EXPECT_THAT(pscs->GetAccessedTopics(), testing::IsEmpty());

  privacy_sandbox::CanonicalTopic topic(browsing_topics::Topic(1),
                                        kTopicsAPITestTaxonomyVersion);
  pscs->OnTopicAccessed(url::Origin::Create(GURL("https://foo.com")), false,
                        topic);
  EXPECT_TRUE(pscs->HasAccessedTopics());
  EXPECT_THAT(pscs->GetAccessedTopics(), testing::Contains(topic));

  // Check that pscs->GetAccessedTopics() does not return the same topic ID
  // twice.
  privacy_sandbox::CanonicalTopic duplicate_topic(
      browsing_topics::Topic(1), kTopicsAPITestTaxonomyVersion - 1);
  pscs->OnTopicAccessed(url::Origin::Create(GURL("https://foo.com")), false,
                        duplicate_topic);
  EXPECT_TRUE(pscs->HasAccessedTopics());
  auto accessed_topics = pscs->GetAccessedTopics();
  EXPECT_EQ(accessed_topics.size(), 1U);
  EXPECT_THAT(accessed_topics, testing::Contains(topic));
}

class PageSpecificContentSettingsWithFencedFrameTest
    : public PageSpecificContentSettingsTest {
 public:
  PageSpecificContentSettingsWithFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~PageSpecificContentSettingsWithFencedFrameTest() override = default;

  content::RenderFrameHost* CreateFencedFrame(const GURL& url) {
    content::RenderFrameHost* fenced_frame_root =
        content::RenderFrameHostTester::For(
            web_contents()->GetPrimaryMainFrame())
            ->AppendFencedFrame();
    std::unique_ptr<content::NavigationSimulator> navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(
            url, fenced_frame_root);
    navigation_simulator->Commit();
    return navigation_simulator->GetFinalRenderFrameHost();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PageSpecificContentSettingsWithFencedFrameTest, SiteDataAccessed) {
  NavigateAndCommit(GURL("http://google.com"));
  const GURL& fenced_frame_url = GURL("http://foo.com");
  content::RenderFrameHost* fenced_frame_root =
      CreateFencedFrame(fenced_frame_url);
  PageSpecificContentSettings* ff_pscs =
      PageSpecificContentSettings::GetForFrame(fenced_frame_root);
  ASSERT_NE(ff_pscs, nullptr);

  // Simulate cookie access in fenced frame.
  {
    MockSiteDataObserver mock_observer(web_contents());
    EXPECT_CALL(mock_observer, OnSiteDataAccessed).Times(1);
    // Set a cookie, block access to images, block mediastream access and block
    // a popup.
    GURL origin("http://google.com");
    std::unique_ptr<net::CanonicalCookie> cookie1(
        net::CanonicalCookie::CreateForTesting(origin, "A=B",
                                               base::Time::Now()));
    ASSERT_TRUE(cookie1);
    ff_pscs->OnCookiesAccessed({content::CookieAccessDetails::Type::kChange,
                                origin,
                                origin,
                                {{*cookie1}},
                                false});
  }
}

TEST_F(PageSpecificContentSettingsWithFencedFrameTest, DelegateUpdatesSent) {
  MockPageSpecificContentSettingsDelegate* mock_delegate =
      InstallMockDelegate();

  const GURL main_url("http://google.com");
  const GURL& ff_url = GURL("http://foo.com");
  const url::Origin& ff_origin = url::Origin::Create(ff_url);

  NavigateAndCommit(main_url);
  content::RenderFrameHost* fenced_frame_root = CreateFencedFrame(ff_url);
  PageSpecificContentSettings* ff_pscs =
      PageSpecificContentSettings::GetForFrame(fenced_frame_root);
  ASSERT_NE(ff_pscs, nullptr);

  EXPECT_CALL(*mock_delegate, OnContentAllowed(ContentSettingsType::COOKIES))
      .Times(1);
  EXPECT_CALL(*mock_delegate, OnContentBlocked(ContentSettingsType::COOKIES))
      .Times(1);

  auto cookie =
      net::CanonicalCookie::CreateForTesting(ff_url, "k=v", base::Time::Now());
  ff_pscs->OnCookiesAccessed({content::CookieAccessDetails::Type::kRead,
                              ff_url,
                              ff_url,
                              {{*cookie}},
                              /*blocked_by_policy=*/false});
  PageSpecificContentSettings::StorageAccessed(
      StorageType::INDEXED_DB, fenced_frame_root->GetGlobalId(),
      fenced_frame_root->GetStorageKey(),
      /*blocked_by_policy=*/true);
}

TEST_F(PageSpecificContentSettingsWithFencedFrameTest,
       ContentAllowedAndBlocked) {
  MockPageSpecificContentSettingsDelegate* mock_delegate =
      InstallMockDelegate();
  NavigateAndCommit(GURL("http://google.com"));
  const GURL& fenced_frame_url = GURL("http://foo.com");
  content::RenderFrameHost* fenced_frame_root =
      CreateFencedFrame(fenced_frame_url);
  PageSpecificContentSettings* ff_pscs =
      PageSpecificContentSettings::GetForFrame(fenced_frame_root);
  ASSERT_NE(ff_pscs, nullptr);

  EXPECT_CALL(*mock_delegate, OnContentAllowed(ContentSettingsType::COOKIES))
      .Times(1);
  EXPECT_CALL(*mock_delegate, OnContentBlocked(ContentSettingsType::JAVASCRIPT))
      .Times(1);

  ff_pscs->OnContentBlocked(ContentSettingsType::JAVASCRIPT);
  ff_pscs->OnContentAllowed(ContentSettingsType::COOKIES);
}

TEST_F(PageSpecificContentSettingsTest,
       MediaIndicatorsBlockedDoNotOverrideInUse) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      content_settings::features::kLeftHandSideActivityIndicators);

  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pscs, nullptr);

  pscs->OnMediaStreamPermissionSet(
      web_contents()->GetLastCommittedURL(),
      {PageSpecificContentSettings::kCameraAccessed});

  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));

  pscs->OnMediaStreamPermissionSet(
      web_contents()->GetLastCommittedURL(),
      {PageSpecificContentSettings::kMicrophoneAccessed,
       PageSpecificContentSettings::kMicrophoneBlocked});

  EXPECT_FALSE(pscs->GetMicrophoneCameraState().HasAny(
      {PageSpecificContentSettings::kMicrophoneAccessed,
       PageSpecificContentSettings::kMicrophoneBlocked}));
}

TEST_F(PageSpecificContentSettingsTest, MediaIndicatorsMinHoldDurationDelay) {
  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pscs, nullptr);

  pscs->set_media_stream_access_origin_for_testing(
      web_contents()->GetLastCommittedURL());

  EXPECT_FALSE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));
  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, true);

  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, false);

  // `kCameraAccessed` is true because of min hold duration.
  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));

  // Min hold duration equals to 5 seconds.
  task_environment()->AdvanceClock(base::Seconds(2));
  base::RunLoop().RunUntilIdle();

  // `kCameraAccessed` is still true because only 2 seconds passed.
  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));

  task_environment()->AdvanceClock(base::Seconds(4));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));
}

// Tests that if media indicator display time almost equals to the min hold
// duration, a delay should be not less than 1 second.
TEST_F(PageSpecificContentSettingsTest, AlmostExpiredMinHoldDurationDelay) {
  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pscs, nullptr);

  pscs->set_media_stream_access_origin_for_testing(
      web_contents()->GetLastCommittedURL());

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, true);

  task_environment()->AdvanceClock(base::Milliseconds(4800));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, false);

  //  `kCameraAccessed` is true because of 1 second delay.
  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));

  task_environment()->AdvanceClock(base::Milliseconds(800));
  base::RunLoop().RunUntilIdle();

  //  `kCameraAccessed` is true because waited only 800 ms.
  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));

  task_environment()->AdvanceClock(base::Milliseconds(300));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));
}

TEST_F(PageSpecificContentSettingsTest,
       MediaIndicatorsHoldAfterUseDurationDelay) {
  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pscs, nullptr);

  pscs->set_media_stream_access_origin_for_testing(
      web_contents()->GetLastCommittedURL());

  EXPECT_FALSE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));
  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, true);

  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));

  // Min hold duration equals to 5 seconds.
  task_environment()->AdvanceClock(base::Seconds(6));
  base::RunLoop().RunUntilIdle();

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, false);

  // `kCameraAccessed` is true because of hold after use duration.
  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));

  // Hold after use duration equals to 1 seconds.
  task_environment()->AdvanceClock(base::Seconds(2));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));
}

TEST_F(PageSpecificContentSettingsTest,
       MediaIndicatorsReenableCameraWhileMinHoldDurationDelay) {
  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pscs, nullptr);

  pscs->set_media_stream_access_origin_for_testing(
      web_contents()->GetLastCommittedURL());

  EXPECT_FALSE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));
  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, true);

  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));

  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, false);

  // `kCameraAccessed` is true because of the min hold duration.
  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));

  // Reenabling a camera indicator
  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, true);

  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));

  task_environment()->AdvanceClock(base::Seconds(6));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kCameraAccessed));
}

// Tests that `PageSpecificContentSettings` will return locally saved last used
// time if it exists.
TEST_F(PageSpecificContentSettingsTest, GetLastUsedTimeLocalTimeTest) {
  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pscs, nullptr);

  base::Time time = base::Time::Now() - base::Seconds(30);

  pscs->set_last_used_time_for_testing(ContentSettingsType::MEDIASTREAM_CAMERA,
                                       time);

  EXPECT_EQ(time,
            pscs->GetLastUsedTime(ContentSettingsType::MEDIASTREAM_CAMERA));
}

TEST_F(PageSpecificContentSettingsTest, GetLastUsedReturnsDefaultTimeTest) {
  NavigateAndCommit(GURL("http://google.com"));

  HostContentSettingsMap* map = settings_map();

  const GURL& url = web_contents()->GetLastCommittedURL();
  map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pscs, nullptr);

  EXPECT_EQ(base::Time(),
            pscs->GetLastUsedTime(ContentSettingsType::MEDIASTREAM_CAMERA));
}

TEST_F(PageSpecificContentSettingsTest, GetLastUsedReturnCorrectTimeTest) {
  NavigateAndCommit(GURL("http://google.com"));

  HostContentSettingsMap* map = settings_map();

  const GURL& url = web_contents()->GetLastCommittedURL();
  map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::MEDIASTREAM_CAMERA, CONTENT_SETTING_ALLOW);

  base::Time time = base::Time::Now() - base::Hours(20);
  map->UpdateLastUsedTime(url, url, ContentSettingsType::MEDIASTREAM_CAMERA,
                          time);

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pscs, nullptr);

  EXPECT_EQ(time,
            pscs->GetLastUsedTime(ContentSettingsType::MEDIASTREAM_CAMERA));
}

// Tests that a permission blocked indicator is visible only for 60 seconds.
TEST_F(PageSpecificContentSettingsTest, MediaBlockedIndicatorsDismissDelay) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {content_settings::features::kImprovedSemanticsActivityIndicators},
      {content_settings::features::kLeftHandSideActivityIndicators});

  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pscs, nullptr);

  EXPECT_FALSE(pscs->get_media_blocked_indicator_timer_for_testing().contains(
      ContentSettingsType::MEDIASTREAM_MIC));

  PageSpecificContentSettings::MicrophoneCameraState
      blocked_microphone_camera_state = {
          PageSpecificContentSettings::kMicrophoneAccessed,
          PageSpecificContentSettings::kMicrophoneBlocked};
  pscs->OnMediaStreamPermissionSet(GURL("http://google.com"),
                                   blocked_microphone_camera_state);

  EXPECT_TRUE(pscs->get_media_blocked_indicator_timer_for_testing().contains(
      ContentSettingsType::MEDIASTREAM_MIC));

  task_environment()->AdvanceClock(base::Seconds(57));
  base::RunLoop().RunUntilIdle();

  // The timer is still there because a delay is 60 seconds.
  EXPECT_TRUE(pscs->get_media_blocked_indicator_timer_for_testing().contains(
      ContentSettingsType::MEDIASTREAM_MIC));

  task_environment()->AdvanceClock(base::Seconds(4));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(pscs->get_media_blocked_indicator_timer_for_testing().contains(
      ContentSettingsType::MEDIASTREAM_MIC));
}

// Tests that a permission indicator will not be dismissed by a timer if was
// opened.
TEST_F(PageSpecificContentSettingsTest,
       MediaIndicatorsDoNotDismissIfOpenedDelay) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      content_settings::features::kImprovedSemanticsActivityIndicators);

  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pscs, nullptr);

  PageSpecificContentSettings::MicrophoneCameraState
      blocked_microphone_camera_state = {
          PageSpecificContentSettings::kMicrophoneAccessed,
          PageSpecificContentSettings::kMicrophoneBlocked};
  pscs->OnMediaStreamPermissionSet(GURL("http://google.com"),
                                   blocked_microphone_camera_state);

  EXPECT_TRUE(pscs->get_media_blocked_indicator_timer_for_testing().contains(
      ContentSettingsType::MEDIASTREAM_MIC));
  // 60 secodns timer is on.
  EXPECT_TRUE(pscs->get_media_blocked_indicator_timer_for_testing()
                  [ContentSettingsType::MEDIASTREAM_MIC]
                      .IsRunning());

  pscs->OnActivityIndicatorBubbleOpened(ContentSettingsType::MEDIASTREAM_MIC);

  EXPECT_TRUE(pscs->get_media_blocked_indicator_timer_for_testing().contains(
      ContentSettingsType::MEDIASTREAM_MIC));
  // A mic timer was stopped.
  EXPECT_FALSE(pscs->get_media_blocked_indicator_timer_for_testing()
                   [ContentSettingsType::MEDIASTREAM_MIC]
                       .IsRunning());

  // A blockage indicator timer was stopped because an activity indicator popup
  // bubble was opened.
  task_environment()->AdvanceClock(base::Seconds(65));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pscs->get_media_blocked_indicator_timer_for_testing().contains(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kMicrophoneAccessed));
  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kMicrophoneBlocked));

  // An indicator popup bubble is closed.
  pscs->OnActivityIndicatorBubbleClosed(ContentSettingsType::MEDIASTREAM_MIC);

  // A mic timer was restarted.
  EXPECT_TRUE(pscs->get_media_blocked_indicator_timer_for_testing()
                  [ContentSettingsType::MEDIASTREAM_MIC]
                      .IsRunning());

  task_environment()->AdvanceClock(base::Seconds(61));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(pscs->get_media_blocked_indicator_timer_for_testing().contains(
      ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kMicrophoneAccessed));
  EXPECT_FALSE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kMicrophoneBlocked));
}

// Tests that a permission blocked state is reset after media started to be
// used.
TEST_F(PageSpecificContentSettingsTest, MediaBlockedStateIsResetIfMediaUsed) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      content_settings::features::kLeftHandSideActivityIndicators);

  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  ASSERT_NE(pscs, nullptr);

  PageSpecificContentSettings::MicrophoneCameraState
      blocked_microphone_camera_state = {
          PageSpecificContentSettings::kMicrophoneAccessed,
          PageSpecificContentSettings::kMicrophoneBlocked};
  pscs->OnMediaStreamPermissionSet(GURL("http://google.com"),
                                   blocked_microphone_camera_state);

  EXPECT_TRUE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kMicrophoneBlocked));

  // Camera is capturing, it should reset blocked microphone state.
  pscs->OnCapturingStateChanged(ContentSettingsType::MEDIASTREAM_CAMERA, true);

  EXPECT_FALSE(pscs->GetMicrophoneCameraState().Has(
      PageSpecificContentSettings::kMicrophoneBlocked));
}

class PageSpecificContentSettingsIframeTest
    : public PageSpecificContentSettingsTest {
 public:
  PageSpecificContentSettingsIframeTest() = default;

  void SetUp() override { PageSpecificContentSettingsTest::SetUp(); }

  // Navigates both the parent and child frames, and then gets content settings
  // from the child frame.
  blink::mojom::RendererContentSettingsPtr NavigateAndGetContentSettings(
      GURL parent_url,
      GURL child_url) {
    NavigateAndCommit(parent_url);

    content::NavigationHandleObserver observer(web_contents(), child_url);
    content::RenderFrameHost* child_rfh =
        content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        child_url, child_rfh);
    simulator->SetTransition(ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
    simulator->Commit();
    return observer.content_settings()->Clone();
  }
};

// Tests that the content settings are correctly set if a secondary url is
// blocked.
TEST_F(PageSpecificContentSettingsIframeTest, SecondaryUrlBlocked) {
  GURL parent_url("https://parent.com");
  GURL child_url("https://child.com");

  settings_map()->SetContentSettingDefaultScope(parent_url, child_url,
                                                ContentSettingsType::JAVASCRIPT,
                                                CONTENT_SETTING_BLOCK);

  blink::mojom::RendererContentSettingsPtr content_settings =
      NavigateAndGetContentSettings(parent_url, child_url);
  EXPECT_FALSE(content_settings->allow_script);
}

// Tests that the content settings are correctly set if an unrelated secondary
// url is blocked.
TEST_F(PageSpecificContentSettingsIframeTest, UnrelatedSecondaryUrlBlocked) {
  GURL other_url("https://other.com");
  GURL parent_url("https://parent.com");
  GURL child_url("https://child.com");

  settings_map()->SetContentSettingDefaultScope(other_url, child_url,
                                                ContentSettingsType::JAVASCRIPT,
                                                CONTENT_SETTING_BLOCK);

  blink::mojom::RendererContentSettingsPtr content_settings =
      NavigateAndGetContentSettings(parent_url, child_url);
  EXPECT_TRUE(content_settings->allow_script);
}

// Tests that the content settings are correctly set if the primary and
// secondary urls are identical.
TEST_F(PageSpecificContentSettingsIframeTest, PrimarySecondaryIdentical) {
  GURL parent_url("https://parent.com");
  GURL child_url = parent_url;
  GURL other_url("https://other.com");

  // All content settings that are sent to the renderer are top-origin scoped.
  // Secondary_url is ignored. This call is functionally equivalent to setting
  // secondary_url = wildcard.
  settings_map()->SetContentSettingDefaultScope(parent_url, other_url,
                                                ContentSettingsType::JAVASCRIPT,
                                                CONTENT_SETTING_BLOCK);

  blink::mojom::RendererContentSettingsPtr content_settings =
      NavigateAndGetContentSettings(parent_url, child_url);
  EXPECT_FALSE(content_settings->allow_script);
}

}  // namespace content_settings
