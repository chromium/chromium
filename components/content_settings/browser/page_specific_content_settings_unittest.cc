// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/page_specific_content_settings.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/content_settings/browser/test_page_specific_content_settings_delegate.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/security_state/core/security_state.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/trust_token_access_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content_settings {

using StorageType = mojom::ContentSettingsManager::StorageType;

namespace {

class MockSiteDataObserver
    : public PageSpecificContentSettings::SiteDataObserver {
 public:
  explicit MockSiteDataObserver(content::WebContents* web_contents)
      : SiteDataObserver(web_contents) {}

  MockSiteDataObserver(const MockSiteDataObserver&) = delete;
  MockSiteDataObserver& operator=(const MockSiteDataObserver&) = delete;

  ~MockSiteDataObserver() override = default;

  MOCK_METHOD0(OnSiteDataAccessed, void());
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
  MOCK_METHOD(void,
              OnCookieAccessAllowed,
              (const net::CookieList& accessed_cookies, content::Page& page));
  MOCK_METHOD(void,
              OnStorageAccessAllowed,
              (StorageType, const url::Origin&, content::Page&));
};

}  // namespace

class PageSpecificContentSettingsTest
    : public content::RenderViewHostTestHarness {
 public:
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
#if !BUILDFLAG(IS_ANDROID)
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
  std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
      origin, "A=B", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(cookie1);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {*cookie1},
                                  false});
  content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
#if !BUILDFLAG(IS_ANDROID)
  content_settings->OnContentBlocked(ContentSettingsType::IMAGES);
#endif
  content_settings->OnContentBlocked(ContentSettingsType::POPUPS);
  PageSpecificContentSettings::MicrophoneCameraState
      blocked_microphone_camera_state =
          PageSpecificContentSettings::MICROPHONE_ACCESSED |
          PageSpecificContentSettings::MICROPHONE_BLOCKED |
          PageSpecificContentSettings::CAMERA_ACCESSED |
          PageSpecificContentSettings::CAMERA_BLOCKED;
  content_settings->OnMediaStreamPermissionSet(
      GURL("http://google.com"), blocked_microphone_camera_state, std::string(),
      std::string(), std::string(), std::string());

  // Check that only the respective content types are affected.
#if !BUILDFLAG(IS_ANDROID)
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
                                  {*cookie1},
                                  false});

  // Block a cookie.
  std::unique_ptr<net::CanonicalCookie> cookie2(net::CanonicalCookie::Create(
      origin, "C=D", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(cookie2);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {*cookie2},
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
#if !BUILDFLAG(IS_ANDROID)
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
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  // Access a file system.
  content_settings->OnStorageAccessed(StorageType::FILE_SYSTEM,
                                      GURL("http://google.com"), false);
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // Block access to a file system.
  content_settings->OnStorageAccessed(StorageType::FILE_SYSTEM,
                                      GURL("http://google.com"), true);
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
  std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
      origin, "A=B", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(cookie1);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {*cookie1},
                                  false});
  ASSERT_TRUE(content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // Record a blocked cookie.
  std::unique_ptr<net::CanonicalCookie> cookie2(net::CanonicalCookie::Create(
      origin, "C=D", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(cookie2);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {*cookie2},
                                  true});
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
       GURL("http://google.com"), net::CookieList(), true});
  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
}

TEST_F(PageSpecificContentSettingsTest, SiteDataObserver) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  MockSiteDataObserver mock_observer(web_contents());
  EXPECT_CALL(mock_observer, OnSiteDataAccessed()).Times(6);

  bool blocked_by_policy = false;
  GURL origin("http://google.com");
  std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
      origin, "A=B", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(cookie);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kChange,
                                  origin,
                                  origin,
                                  {*cookie},
                                  blocked_by_policy});

  net::CookieList cookie_list;
  std::unique_ptr<net::CanonicalCookie> other_cookie(
      net::CanonicalCookie::Create(GURL("http://google.com"),
                                   "CookieName=CookieValue", base::Time::Now(),
                                   absl::nullopt /* server_time */,
                                   absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(other_cookie);

  cookie_list.push_back(*other_cookie);
  GetHandle()->OnCookiesAccessed(
      web_contents()->GetPrimaryMainFrame(),
      {content::CookieAccessDetails::Type::kRead, GURL("http://google.com"),
       GURL("http://google.com"), cookie_list, blocked_by_policy});
  content_settings->OnStorageAccessed(
      StorageType::FILE_SYSTEM, GURL("http://google.com"), blocked_by_policy);
  content_settings->OnStorageAccessed(
      StorageType::INDEXED_DB, GURL("http://google.com"), blocked_by_policy);
  content_settings->OnStorageAccessed(
      StorageType::LOCAL_STORAGE, GURL("http://google.com"), blocked_by_policy);
  content_settings->OnStorageAccessed(
      StorageType::DATABASE, GURL("http://google.com"), blocked_by_policy);
}

TEST_F(PageSpecificContentSettingsTest, LocalSharedObjectsContainer) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  bool blocked_by_policy = false;
  auto cookie = net::CanonicalCookie::Create(
      GURL("http://google.com"), "k=v", base::Time::Now(),
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kRead,
                                  GURL("http://google.com"),
                                  GURL("http://google.com"),
                                  {*cookie},
                                  blocked_by_policy});
  content_settings->OnStorageAccessed(StorageType::FILE_SYSTEM,
                                      GURL("https://www.google.com"),
                                      blocked_by_policy);
  content_settings->OnStorageAccessed(
      StorageType::INDEXED_DB, GURL("https://localhost"), blocked_by_policy);
  content_settings->OnStorageAccessed(StorageType::LOCAL_STORAGE,
                                      GURL("http://maps.google.com:8080"),
                                      blocked_by_policy);
  content_settings->OnStorageAccessed(StorageType::LOCAL_STORAGE,
                                      GURL("http://example.com"),
                                      blocked_by_policy);
  content_settings->OnStorageAccessed(
      StorageType::DATABASE, GURL("http://192.168.0.1"), blocked_by_policy);
  content_settings->OnSharedWorkerAccessed(
      GURL("http://youtube.com/worker.js"), "worker",
      blink::StorageKey::CreateFromStringForTesting("https://youtube.com"),
      blocked_by_policy);

  const auto& objects = content_settings->allowed_local_shared_objects();
  EXPECT_EQ(7u, objects.GetObjectCount());
  EXPECT_EQ(3u, objects.GetObjectCountForDomain(GURL("http://google.com")));
  EXPECT_EQ(1u, objects.GetObjectCountForDomain(GURL("http://youtube.com")));
  EXPECT_EQ(1u, objects.GetObjectCountForDomain(GURL("http://localhost")));
  EXPECT_EQ(1u, objects.GetObjectCountForDomain(GURL("http://example.com")));
  EXPECT_EQ(1u, objects.GetObjectCountForDomain(GURL("http://192.168.0.1")));
  // google.com, www.google.com, localhost, maps.google.com, example.com,
  // youtube.com, 192.168.0.1 should be counted as hosts.
  EXPECT_EQ(7u, objects.GetHostCount());

  // The localStorage storage keys (http://maps.google.com:8080 and
  // http://example.com) should be ignored since they are empty.
  base::RunLoop run_loop;
  objects.UpdateIgnoredEmptyStorageKeys(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(5u, objects.GetObjectCount());
  EXPECT_EQ(2u, objects.GetObjectCountForDomain(GURL("http://google.com")));
  EXPECT_EQ(0u, objects.GetObjectCountForDomain(GURL("http://example.com")));
}

TEST_F(PageSpecificContentSettingsTest, LocalSharedObjectsContainerCookie) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  bool blocked_by_policy = false;
  auto cookie1 = net::CanonicalCookie::Create(
      GURL("http://google.com"), "k1=v", base::Time::Now(),
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  auto cookie2 = net::CanonicalCookie::Create(
      GURL("http://www.google.com"), "k2=v; Domain=google.com",
      base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  auto cookie3 = net::CanonicalCookie::Create(
      GURL("http://www.google.com"), "k3=v; Domain=.google.com",
      base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  auto cookie4 = net::CanonicalCookie::Create(
      GURL("http://www.google.com"), "k4=v; Domain=.www.google.com",
      base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kRead,
                                  GURL("http://www.google.com"),
                                  GURL("http://www.google.com"),
                                  {*cookie1, *cookie2, *cookie3, *cookie4},
                                  blocked_by_policy});

  auto cookie5 = net::CanonicalCookie::Create(
      GURL("https://www.google.com"), "k5=v", base::Time::Now(),
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kRead,
                                  GURL("https://www.google.com"),
                                  GURL("https://www.google.com"),
                                  {*cookie5},
                                  blocked_by_policy});

  const auto& objects = content_settings->allowed_local_shared_objects();
  EXPECT_EQ(5u, objects.GetObjectCount());
  EXPECT_EQ(5u, objects.GetObjectCountForDomain(GURL("http://google.com")));
  // google.com and www.google.com
  EXPECT_EQ(2u, objects.GetHostCount());
}

TEST_F(PageSpecificContentSettingsTest, BrowsingDataModelTrustToken) {
  NavigateAndCommit(GURL("http://google.com"));

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  auto* allowed_browsing_data_model = pscs->allowed_browsing_data_model();

  // Before Trust Token accesses, there should be no objects here.
  EXPECT_EQ(
      0, browsing_data::GetUniqueHostCount(pscs->allowed_local_shared_objects(),
                                           *allowed_browsing_data_model));
  const url::Origin origin = url::Origin::Create(GURL("http://google.com/"));
  const url::Origin issuer =
      url::Origin::Create(GURL("http://issuer.example/"));
  // Access a Trust Token.
  GetHandle()->OnTrustTokensAccessed(
      web_contents()->GetPrimaryMainFrame(),
      content::TrustTokenAccessDetails(
          origin, network::mojom::TrustTokenOperationType::kIssuance, issuer,
          false));

  EXPECT_EQ(
      1, browsing_data::GetUniqueHostCount(pscs->allowed_local_shared_objects(),
                                           *allowed_browsing_data_model));
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
  EXPECT_EQ(
      1, browsing_data::GetUniqueHostCount(pscs->allowed_local_shared_objects(),
                                           *allowed_browsing_data_model));
}

TEST_F(PageSpecificContentSettingsTest, LocalSharedObjectsContainerHostsCount) {
  NavigateAndCommit(GURL("http://google.com"));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  bool blocked_by_policy = false;
  auto cookie1 = net::CanonicalCookie::Create(
      GURL("http://google.com"), "k1=v", base::Time::Now(),
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  auto cookie2 = net::CanonicalCookie::Create(
      GURL("https://example.com"), "k2=v", base::Time::Now(),
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  auto cookie3 = net::CanonicalCookie::Create(
      GURL("https://example.com"), "k3=v", base::Time::Now(),
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  auto cookie4 = net::CanonicalCookie::Create(
      GURL("http://example.com"), "k4=v", base::Time::Now(),
      absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kRead,
                                  GURL("http://google.com"),
                                  GURL("http://google.com"),
                                  {*cookie1},
                                  blocked_by_policy});
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kRead,
                                  GURL("https://example.com"),
                                  GURL("https://example.com"),
                                  {*cookie2},
                                  blocked_by_policy});
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kRead,
                                  GURL("http://example.com"),
                                  GURL("http://example.com"),
                                  {*cookie3},
                                  blocked_by_policy});
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kRead,
                                  GURL("http://example.com"),
                                  GURL("http://example.com"),
                                  {*cookie4},
                                  blocked_by_policy});
  content_settings->OnStorageAccessed(StorageType::FILE_SYSTEM,
                                      GURL("https://www.google.com"),
                                      blocked_by_policy);
  content_settings->OnStorageAccessed(
      StorageType::INDEXED_DB, GURL("https://localhost"), blocked_by_policy);
  content_settings->OnStorageAccessed(StorageType::LOCAL_STORAGE,
                                      GURL("http://maps.google.com:8080"),
                                      blocked_by_policy);
  content_settings->OnStorageAccessed(StorageType::LOCAL_STORAGE,
                                      GURL("http://example.com"),
                                      blocked_by_policy);
  content_settings->OnStorageAccessed(
      StorageType::DATABASE, GURL("http://192.168.0.1"), blocked_by_policy);
  content_settings->OnSharedWorkerAccessed(
      GURL("http://youtube.com/worker.js"), "worker",
      blink::StorageKey::CreateFromStringForTesting("https://youtube.com"),
      blocked_by_policy);

  const auto& objects = content_settings->allowed_local_shared_objects();
  EXPECT_EQ(10u, objects.GetObjectCount());
  EXPECT_EQ(7u, objects.GetHostCount());
  EXPECT_EQ(3u, objects.GetHostCountForDomain(GURL("http://google.com")));
  EXPECT_EQ(1u, objects.GetHostCountForDomain(GURL("http://youtube.com")));
  EXPECT_EQ(3u, objects.GetHostCountForDomain(GURL("http://a.google.com")));
  EXPECT_EQ(1u, objects.GetHostCountForDomain(GURL("http://a.example.com")));
}

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

TEST_F(PageSpecificContentSettingsTest, AllowedSitesCountedFromBothModels) {
  // Populate containers with hosts.
  bool blocked_by_policy = false;
  auto googleURL = GURL("http://google.com");
  auto exampleURL = GURL("https://example.com");
  auto cookie1 = net::CanonicalCookie::Create(
      googleURL, "k1=v", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  auto cookie2 = net::CanonicalCookie::Create(
      exampleURL, "k2=v", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kRead,
                                  googleURL,
                                  googleURL,
                                  {*cookie1},
                                  blocked_by_policy});
  GetHandle()->OnCookiesAccessed(web_contents()->GetPrimaryMainFrame(),
                                 {content::CookieAccessDetails::Type::kRead,
                                  exampleURL,
                                  exampleURL,
                                  {*cookie2},
                                  blocked_by_policy});

  PageSpecificContentSettings* pscs = PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());

  auto* allowed_browsing_data_model = pscs->allowed_browsing_data_model();
  allowed_browsing_data_model->AddBrowsingData(
      url::Origin::Create(exampleURL),
      BrowsingDataModel::StorageType::kTrustTokens, /*storage_size=*/0);

  // Verify the size is counted without duplication of hosts.
  EXPECT_EQ(
      2, browsing_data::GetUniqueHostCount(pscs->allowed_local_shared_objects(),
                                           *allowed_browsing_data_model));
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
    EXPECT_CALL(mock_observer, OnSiteDataAccessed()).Times(0);
    // Set a cookie, block access to images, block mediastream access and block
    // a popup.
    GURL origin("http://google.com");
    std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
        origin, "A=B", base::Time::Now(), absl::nullopt /* server_time */,
        absl::nullopt /* cookie_partition_key */));
    ASSERT_TRUE(cookie1);
    pscs->OnCookiesAccessed({content::CookieAccessDetails::Type::kChange,
                             origin,
                             origin,
                             {*cookie1},
                             false});
  }
  // Activate prerendering page.
  {
    MockSiteDataObserver mock_observer(web_contents());
    // OnSiteDataAccessed should be called after page is activated.
    EXPECT_CALL(mock_observer, OnSiteDataAccessed()).Times(1);
    std::unique_ptr<content::NavigationSimulator> navigation =
        content::NavigationSimulator::CreateRendererInitiated(
            prerender_url, web_contents()->GetPrimaryMainFrame());
    // TODO(https://crbug.com/1181763): Investigate how default referrer value
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

  EXPECT_CALL(*mock_delegate, OnCookieAccessAllowed).Times(0);
  EXPECT_CALL(*mock_delegate, OnStorageAccessAllowed).Times(0);

  const bool blocked_by_policy = false;
  const GURL url = GURL("http://google.com");
  const url::Origin origin = url::Origin::Create(url);
  auto cookie = net::CanonicalCookie::Create(
      url, "k=v", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  pscs->OnCookiesAccessed({content::CookieAccessDetails::Type::kRead,
                           url,
                           url,
                           {*cookie},
                           blocked_by_policy});
  pscs->OnStorageAccessed(StorageType::INDEXED_DB, url, blocked_by_policy);
  pscs->OnStorageAccessed(StorageType::LOCAL_STORAGE, url, blocked_by_policy);
  pscs->OnStorageAccessed(StorageType::DATABASE, url, blocked_by_policy);

  content::Page& prerender_page = prerender_frame->GetPage();
  EXPECT_CALL(*mock_delegate,
              OnCookieAccessAllowed(testing::_, testing::Ref(prerender_page)))
      .Times(1);
  EXPECT_CALL(*mock_delegate,
              OnStorageAccessAllowed(StorageType::INDEXED_DB, origin,
                                     testing::Ref(prerender_page)))
      .Times(1);
  EXPECT_CALL(*mock_delegate,
              OnStorageAccessAllowed(StorageType::LOCAL_STORAGE, origin,
                                     testing::Ref(prerender_page)))
      .Times(1);
  EXPECT_CALL(*mock_delegate,
              OnStorageAccessAllowed(StorageType::DATABASE, origin,
                                     testing::Ref(prerender_page)))
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
  // TODO(https://crbug.com/1181763): Investigate how default referrer value is
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

  privacy_sandbox::CanonicalTopic topic(
      browsing_topics::Topic(1),
      privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY);
  pscs->OnTopicAccessed(url::Origin::Create(GURL("https://foo.com")), false,
                        topic);
  EXPECT_TRUE(pscs->HasAccessedTopics());
  EXPECT_THAT(pscs->GetAccessedTopics(), testing::Contains(topic));
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
    EXPECT_CALL(mock_observer, OnSiteDataAccessed()).Times(1);
    // Set a cookie, block access to images, block mediastream access and block
    // a popup.
    GURL origin("http://google.com");
    std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
        origin, "A=B", base::Time::Now(), absl::nullopt /* server_time */,
        absl::nullopt /* cookie_partition_key */));
    ASSERT_TRUE(cookie1);
    ff_pscs->OnCookiesAccessed({content::CookieAccessDetails::Type::kChange,
                                origin,
                                origin,
                                {*cookie1},
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

  content::Page& ff_page = fenced_frame_root->GetPage();
  EXPECT_CALL(*mock_delegate,
              OnCookieAccessAllowed(testing::_, testing::Ref(ff_page)))
      .Times(1);
  EXPECT_CALL(*mock_delegate,
              OnStorageAccessAllowed(StorageType::INDEXED_DB, ff_origin,
                                     testing::Ref(ff_page)))
      .Times(1);
  EXPECT_CALL(*mock_delegate,
              OnStorageAccessAllowed(StorageType::LOCAL_STORAGE, ff_origin,
                                     testing::Ref(ff_page)))
      .Times(1);
  EXPECT_CALL(*mock_delegate,
              OnStorageAccessAllowed(StorageType::DATABASE, ff_origin,
                                     testing::Ref(ff_page)))
      .Times(1);

  const bool blocked_by_policy = false;
  auto cookie = net::CanonicalCookie::Create(
      ff_url, "k=v", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  ff_pscs->OnCookiesAccessed({content::CookieAccessDetails::Type::kRead,
                              ff_url,
                              ff_url,
                              {*cookie},
                              blocked_by_policy});
  ff_pscs->OnStorageAccessed(StorageType::INDEXED_DB, ff_url,
                             blocked_by_policy);
  ff_pscs->OnStorageAccessed(StorageType::LOCAL_STORAGE, ff_url,
                             blocked_by_policy);
  ff_pscs->OnStorageAccessed(StorageType::DATABASE, ff_url, blocked_by_policy);
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

}  // namespace content_settings
