// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/ui/cookie_controls_controller.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/fingerprinting_protection/chrome_fingerprinting_protection_web_contents_helper_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/cookie_blocking_3pcd_status.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/cookie_controls_state.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/common/third_party_site_data_access_type.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/ip_protection/common/ip_protection_status.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom-shared.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_string_util.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using StorageType =
    content_settings::mojom::ContentSettingsManager::StorageType;

constexpr char kCookieControlsActivatedSaaHistogram[] =
    "Privacy.CookieControlsActivated.SaaRequested";
constexpr char kCookieControlsActivatedRefreshCountHistogram[] =
    "Privacy.CookieControlsActivated.PageRefreshCount";
constexpr char kCookieControlsActivatedSiteEngagementHistogram[] =
    "Privacy.CookieControlsActivated.SiteEngagementScore";
constexpr char kCookieControlsActivatedSiteDataAccessHistogram[] =
    "Privacy.CookieControlsActivated.SiteDataAccessType";
constexpr char kUrl[] = "https://example.com";

class MockCookieControlsObserver
    : public content_settings::CookieControlsObserver {
 public:
  MOCK_METHOD(void,
              OnStatusChanged,
              (CookieControlsState,
               CookieControlsEnforcement,
               CookieBlocking3pcdStatus,
               base::Time));
  MOCK_METHOD(void,
              OnCookieControlsIconStatusChanged,
              (/*icon_visible*/ bool,
               CookieControlsState,
               CookieBlocking3pcdStatus,
               /*should_highlight*/ bool));
  MOCK_METHOD(void, OnFinishedPageReloadWithChangedSettings, ());
  MOCK_METHOD(void, OnReloadThresholdExceeded, ());
};

blink::StorageKey CreateUnpartitionedStorageKey(const GURL& url) {
  return blink::StorageKey::CreateFirstParty(url::Origin::Create(url));
}

}  // namespace

std::ostream& operator<<(std::ostream& os,
                         const CookieControlsEnforcement& enforcement) {
  switch (enforcement) {
    case CookieControlsEnforcement::kNoEnforcement:
      return os << "kNoEnforcement";
    case CookieControlsEnforcement::kEnforcedByCookieSetting:
      return os << "kEnforcedByCookieSetting";
    case CookieControlsEnforcement::kEnforcedByExtension:
      return os << "kEnforcedByExtension";
    case CookieControlsEnforcement::kEnforcedByPolicy:
      return os << "kEnforcedByPolicy";
    case CookieControlsEnforcement::kEnforcedByTpcdGrant:
      return os << "kEnforcedByTpcdGrant";
  }
}

class CookieControlsUserBypassTest : public ChromeRenderViewHostTestHarness {
 public:
  CookieControlsUserBypassTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // NOTE: we make the exception short (hours rather than days) to prevent it
    // from timing out
    feature_list_.InitWithFeaturesAndParameters(
        {{content_settings::features::kUserBypassUI,
          {{"expiration", "3h"}, {"reload-count", "2"}}}},
        {});
  }

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
    profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
    NavigateAndCommit(GURL("chrome://newtab"));

    cookie_settings_ = CookieSettingsFactory::GetForProfile(profile());
    tracking_protection_settings_ =
        TrackingProtectionSettingsFactory::GetForProfile(profile());
    cookie_controls_ =
        std::make_unique<content_settings::CookieControlsController>(
            cookie_settings_, nullptr,
            HostContentSettingsMapFactory::GetForProfile(profile()),
            tracking_protection_settings_,
            /*is_incognito_profile=*/false);
    cookie_controls_->AddObserver(mock());
    testing::Mock::VerifyAndClearExpectations(mock());

    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void TearDown() override {
    cookie_controls_->RemoveObserver(mock());
    cookie_controls_.reset();
    tracking_protection_settings_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void ValidateCookieControlsActivatedUKM(
      bool fed_cm_initiated,
      bool storage_access_api_requested,
      int page_refresh_count,
      bool repeated_activation,
      blink::mojom::EngagementLevel site_engagement_level,
      ThirdPartySiteDataAccessType site_data_access_type) {
    auto entries = ukm_recorder_->GetEntriesByName(
        "ThirdPartyCookies.CookieControlsActivated");
    ASSERT_EQ(1u, entries.size());
    auto* entry = entries.front().get();

    ukm_recorder_->ExpectEntryMetric(entry, "FedCmInitiated", fed_cm_initiated);
    ukm_recorder_->ExpectEntryMetric(entry, "StorageAccessAPIRequested",
                                     storage_access_api_requested);
    ukm_recorder_->ExpectEntryMetric(entry, "PageRefreshCount",
                                     page_refresh_count);
    ukm_recorder_->ExpectEntryMetric(entry, "RepeatedActivation",
                                     repeated_activation);
    ukm_recorder_->ExpectEntryMetric(
        entry, "SiteEngagementLevel",
        static_cast<uint64_t>(site_engagement_level));
    ukm_recorder_->ExpectEntryMetric(
        entry, "ThirdPartySiteDataAccessType",
        static_cast<uint64_t>(site_data_access_type));

    // Ideally we would check the associated URL directly, but that is
    // evidently non-trivial, so we settle for making sure the right ID was
    // used.
    EXPECT_EQ(web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId(),
              entry->source_id);

    // Reset the recorder, tests should check every UKM report they expect.
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  MockCookieControlsObserver* mock() { return &mock_; }

  base::Time zero_expiration() const { return base::Time(); }

  base::Time expiration() const {
    auto delta =
        content_settings::features::kUserBypassUIExceptionExpiration.Get();
    return delta.is_zero() ? base::Time() : base::Time::Now() + delta;
  }

  content_settings::CookieControlsController* cookie_controls() {
    return cookie_controls_.get();
  }

  content_settings::CookieSettings* cookie_settings() {
    return cookie_settings_.get();
  }

  privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings() {
    return tracking_protection_settings_;
  }

  content_settings::PageSpecificContentSettings*
  page_specific_content_settings() {
    return content_settings::PageSpecificContentSettings::GetForFrame(
        web_contents()->GetPrimaryMainFrame());
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
  }

  void FastForwardTo(base::Time target) {
    task_environment()->FastForwardBy(target - base::Time::Now());
  }

  blink::mojom::ResourceLoadInfoPtr
  CreateResourceLoadInfoWithIpProtectionChain() {
    blink::mojom::ResourceLoadInfoPtr resource_load_info =
        blink::mojom::ResourceLoadInfo::New();

    resource_load_info->proxy_chain = net::ProxyChain::ForIpProtection(
        {net::ProxyUriToProxyServer("foo:555", net::ProxyServer::SCHEME_HTTPS),
         net::ProxyUriToProxyServer("foo:666",
                                    net::ProxyServer::SCHEME_HTTPS)});
    return resource_load_info;
  }

 private:
  MockCookieControlsObserver mock_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  std::unique_ptr<content_settings::CookieControlsController> cookie_controls_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<privacy_sandbox::TrackingProtectionSettings>
      tracking_protection_settings_;
};

TEST_F(CookieControlsUserBypassTest, CookieBlockingChanged) {
  // Check that the controller correctly keeps track of whether the effective
  // cookie blocking setting for the page has been changed by
  // `SetUserChangedCookieBlockingForSite`.
  cookie_controls()->Update(web_contents());
  NavigateAndCommit(GURL(kUrl));
  EXPECT_FALSE(cookie_controls()->HasUserChangedCookieBlockingForSite());

  cookie_controls()->SetUserChangedCookieBlockingForSite(false);
  EXPECT_FALSE(cookie_controls()->HasUserChangedCookieBlockingForSite());

  cookie_controls()->SetUserChangedCookieBlockingForSite(true);
  EXPECT_TRUE(cookie_controls()->HasUserChangedCookieBlockingForSite());

  // Changing the toggle back should clear it.
  cookie_controls()->SetUserChangedCookieBlockingForSite(true);
  EXPECT_FALSE(cookie_controls()->HasUserChangedCookieBlockingForSite());

  // Navigating to the same page should clear it.
  cookie_controls()->SetUserChangedCookieBlockingForSite(true);
  EXPECT_TRUE(cookie_controls()->HasUserChangedCookieBlockingForSite());
  NavigateAndCommit(GURL(kUrl));
  EXPECT_FALSE(cookie_controls()->HasUserChangedCookieBlockingForSite());

  // Navigating to a different page should also clear it.
  cookie_controls()->SetUserChangedCookieBlockingForSite(true);
  EXPECT_TRUE(cookie_controls()->HasUserChangedCookieBlockingForSite());
  NavigateAndCommit(GURL("https://thirdparty.com"));
  EXPECT_FALSE(cookie_controls()->HasUserChangedCookieBlockingForSite());
}

TEST_F(CookieControlsUserBypassTest, SiteCounts) {
  base::HistogramTester t;

  // Visiting a website should enable the UI.
  NavigateAndCommit(GURL(kUrl));

  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should be notified.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://anotherthirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/true);

  // Enabling third-party cookies records metrics.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           CookieControlsState::kAllowed3pc,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  t.ExpectUniqueSample(kCookieControlsActivatedSaaHistogram, false, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedRefreshCountHistogram, 0, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedSiteEngagementHistogram, 0, 1);
  t.ExpectUniqueSample(
      kCookieControlsActivatedSiteDataAccessHistogram,
      ThirdPartySiteDataAccessType::kAnyBlockedThirdPartySiteAccesses, 1);
}

TEST_F(CookieControlsUserBypassTest, NewTabPage) {
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kHidden,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, CookieControlsState::kHidden,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
}

TEST_F(CookieControlsUserBypassTest, PreferenceDisabled) {
  NavigateAndCommit(GURL(kUrl));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling the feature should disable the UI.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kHidden,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, CookieControlsState::kHidden,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));
  testing::Mock::VerifyAndClearExpectations(mock());
}
TEST_F(CookieControlsUserBypassTest, AllCookiesBlocked) {
  base::HistogramTester t;
  NavigateAndCommit(GURL(kUrl));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disable all cookies - an OnStatusCallback should get triggered.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disable cookie blocking for example.com.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           CookieControlsState::kAllowed3pc,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  t.ExpectUniqueSample(kCookieControlsActivatedSaaHistogram, false, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedRefreshCountHistogram, 0, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedSiteEngagementHistogram, 0, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedSiteDataAccessHistogram,
                       ThirdPartySiteDataAccessType::kNoThirdPartySiteAccesses,
                       1);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, DisableForSite) {
  NavigateAndCommit(GURL(kUrl));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling cookie blocking for example.com should update the ui.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           CookieControlsState::kAllowed3pc,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Visiting some other site, should re-enable protections.
  NavigateAndCommit(GURL("https://somethingelse.com"));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Visiting example.com should turn protections off.
  NavigateAndCommit(GURL(kUrl));
  EXPECT_CALL(*mock(), OnStatusChanged(
                           CookieControlsState::kAllowed3pc,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling example.com again should re-enable protections.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, Incognito) {
  NavigateAndCommit(GURL(kUrl));

  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Create incognito web_contents and CookieControlsController.
  std::unique_ptr<content::WebContents> incognito_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), nullptr);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      incognito_web_contents.get(),
      std::make_unique<PageSpecificContentSettingsDelegate>(
          incognito_web_contents.get()));
  auto* tester = content::WebContentsTester::For(incognito_web_contents.get());
  MockCookieControlsObserver incognito_mock;
  content_settings::CookieControlsController incognito_cookie_controls(
      CookieSettingsFactory::GetForProfile(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)),
      CookieSettingsFactory::GetForProfile(profile()),
      HostContentSettingsMapFactory::GetForProfile(profile()),
      TrackingProtectionSettingsFactory::GetForProfile(profile()),
      /*is_incognito_profile=*/true);
  incognito_cookie_controls.AddObserver(&incognito_mock);

  // Navigate incognito web_contents to the same URL.
  tester->NavigateAndCommit(GURL(kUrl));
  EXPECT_CALL(
      incognito_mock,
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(
      incognito_mock,
      OnCookieControlsIconStatusChanged(
          /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
          CookieBlocking3pcdStatus::kNotIn3pcd, /*should_highlight=*/false));
  incognito_cookie_controls.Update(incognito_web_contents.get());
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(&incognito_mock);

  // Allow cookies in regular mode should also allow in incognito but enforced
  // through regular mode.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           CookieControlsState::kAllowed3pc,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));

  EXPECT_CALL(
      incognito_mock,
      OnStatusChanged(CookieControlsState::kAllowed3pc,
                      CookieControlsEnforcement::kEnforcedByCookieSetting,
                      CookieBlocking3pcdStatus::kNotIn3pcd, expiration()));
  EXPECT_CALL(
      incognito_mock,
      OnCookieControlsIconStatusChanged(
          /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
          CookieBlocking3pcdStatus::kNotIn3pcd, /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(&incognito_mock);

  // This should be enforced regardless of the default cookie setting in the
  // default profile.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsState::kHidden,
                              CookieControlsEnforcement::kNoEnforcement,
                              CookieBlocking3pcdStatus::kNotIn3pcd,
                              // Although there is an allow exception with an
                              // expiration, because the default allow never
                              // expires, zero_expiration is correct.
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, CookieControlsState::kHidden,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));

  EXPECT_CALL(
      incognito_mock,
      OnStatusChanged(CookieControlsState::kAllowed3pc,
                      CookieControlsEnforcement::kEnforcedByCookieSetting,
                      CookieBlocking3pcdStatus::kNotIn3pcd, expiration()));
  EXPECT_CALL(
      incognito_mock,
      OnCookieControlsIconStatusChanged(
          /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
          CookieBlocking3pcdStatus::kNotIn3pcd, /*should_highlight=*/false));
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kIncognitoOnly));
  incognito_cookie_controls.Update(incognito_web_contents.get());
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(&incognito_mock);
}

TEST_F(CookieControlsUserBypassTest, ThirdPartyCookiesException) {
  // Create third party cookies exception.
  cookie_settings()->SetThirdPartyCookieSetting(
      GURL(kUrl), ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateAndCommit(GURL(kUrl));
  // Third-party cookie exceptions are handled in the same way as exceptions
  // created for user bypass.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kAllowed3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling 3PC for example.com again should change status to kEnabled.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, FrequentPageReloads) {
  // Update on the initial web contents to ensure the tab observer is setup.
  cookie_controls()->Update(web_contents());
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());

  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  NavigateAndCommit(GURL(kUrl));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should be notified.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  page_specific_content_settings()->OnBrowsingDataAccessed(

      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Reload the page and simulate accessing storage on page load.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  NavigateAndCommit(GURL(kUrl));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // After the second reload and accessing storage, UB should highlight.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/true));

  NavigateAndCommit(GURL(kUrl));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  cookie_controls()->OnEntryPointAnimated();
  testing::Mock::VerifyAndClearExpectations(mock());

  // After the entry point was highlighted, a setting is recorded.
  base::Value stored_value = hcsm->GetWebsiteSetting(
      GURL(kUrl), GURL(), ContentSettingsType::COOKIE_CONTROLS_METADATA);
  EXPECT_TRUE(stored_value.is_dict());
  EXPECT_TRUE(stored_value.GetDict().FindBool("entry_point_animated").value());
}

TEST_F(CookieControlsUserBypassTest,
       HittingPageReloadThresholdTriggersOnReloadThresholdExceeded) {
  // Update initial web contents to ensure the tab observer is set up.
  cookie_controls()->Update(web_contents());

  // Don't call observer when reload count = 0.
  EXPECT_CALL(*mock(), OnReloadThresholdExceeded()).Times(0);

  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  NavigateAndCommit(GURL(kUrl));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Don't call observer when reload count = 1.
  EXPECT_CALL(*mock(), OnReloadThresholdExceeded()).Times(0);

  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  NavigateAndCommit(GURL(kUrl));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Expect observer call when reload count hits threshold of 2.
  EXPECT_CALL(*mock(), OnReloadThresholdExceeded());
  // Expect that we attempt to highlight the user bypass icon.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/true));
  NavigateAndCommit(GURL(kUrl));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest,
       UserBypassDoesNotHighlightIfCookiesAreAllowed) {
  // Set cookie blocking pref to allow all cookies.
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));

  // Update initial web contents to ensure the tab observer is set up.
  cookie_controls()->Update(web_contents());

  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kHidden,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, CookieControlsState::kHidden,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  NavigateAndCommit(GURL(kUrl));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());
  // Trigger reload heuristic.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, CookieControlsState::kHidden,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  NavigateAndCommit(GURL(kUrl));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Verify we do not attempt to highlight user bypass as 3PCs are allowed.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, CookieControlsState::kHidden,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  NavigateAndCommit(GURL(kUrl));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, FrequentPageReloadsMetrics) {
  base::HistogramTester t;
  cookie_controls()->Update(web_contents());

  NavigateAndCommit(GURL(kUrl));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should be notified.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Reload the page and simulate accessing storage on page load.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  NavigateAndCommit(GURL(kUrl));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // After the second reload and accessing storage, UB should be highlighted.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/true));
  NavigateAndCommit(GURL(kUrl));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling third-party cookies records metrics.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           CookieControlsState::kAllowed3pc,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  t.ExpectUniqueSample(kCookieControlsActivatedSaaHistogram, false, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedRefreshCountHistogram, 2, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedSiteEngagementHistogram, 0, 1);
  t.ExpectUniqueSample(
      kCookieControlsActivatedSiteDataAccessHistogram,
      ThirdPartySiteDataAccessType::kAnyAllowedThirdPartySiteAccesses, 1);
  ValidateCookieControlsActivatedUKM(
      /*fed_cm_initiated=*/false,
      /*storage_access_api_requested=*/false,
      /*page_refresh_count=*/2,  // Count was reset to 0 after timeout.
      /*repeated_activation=*/false, blink::mojom::EngagementLevel::NONE,
      ThirdPartySiteDataAccessType::kAnyAllowedThirdPartySiteAccesses);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, InfrequentPageReloads) {
  base::HistogramTester t;
  NavigateAndCommit(GURL(kUrl));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should be notified.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  page_specific_content_settings()->OnBrowsingDataAccessed(

      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Reload the page and simulate accessing storage on page load.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  NavigateAndCommit(GURL(kUrl));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Wait for 30 seconds.
  FastForwardBy(base::Seconds(30));

  // The second reload happens with a delay and doesn't highlight.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  NavigateAndCommit(GURL(kUrl));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling third-party cookies records metrics.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           CookieControlsState::kAllowed3pc,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  t.ExpectUniqueSample(kCookieControlsActivatedSaaHistogram, false, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedRefreshCountHistogram, 1, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedSiteEngagementHistogram, 0, 1);
  t.ExpectUniqueSample(
      kCookieControlsActivatedSiteDataAccessHistogram,
      ThirdPartySiteDataAccessType::kAnyAllowedThirdPartySiteAccesses, 1);
  ValidateCookieControlsActivatedUKM(
      /*fed_cm_initiated=*/false,
      /*storage_access_api_requested=*/false,
      /*page_refresh_count=*/1,  // Count was reset to 0 after timeout.
      /*repeated_activation=*/false, blink::mojom::EngagementLevel::NONE,
      ThirdPartySiteDataAccessType::kAnyAllowedThirdPartySiteAccesses);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, HighSiteEngagement) {
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());

  // An engagement score above HIGH.
  const int kHighEngagement = 60;
  // An engagement score below MEDIUM.
  const int kLowEngagement = 1;

  site_engagement::SiteEngagementService::Get(profile())->ResetBaseScoreForURL(
      GURL("https://highengagement.com"), kHighEngagement);
  site_engagement::SiteEngagementService::Get(profile())->ResetBaseScoreForURL(
      GURL("https://somethingelse.com"), kLowEngagement);

  NavigateAndCommit(GURL("https://highengagement.com"));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/true));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Site data access should highlight.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/true));
  page_specific_content_settings()->OnBrowsingDataAccessed(

      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  cookie_controls()->OnEntryPointAnimated();
  testing::Mock::VerifyAndClearExpectations(mock());

  // After the entry point was highlighted a setting is recorded.
  base::Value stored_value =
      hcsm->GetWebsiteSetting(GURL("https://highengagement.com"), GURL(),
                              ContentSettingsType::COOKIE_CONTROLS_METADATA);
  EXPECT_TRUE(stored_value.is_dict());
  EXPECT_TRUE(stored_value.GetDict().FindBool("entry_point_animated").value());

  // Visiting some other site should reset the state.
  NavigateAndCommit(GURL("https://somethingelse.com"));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Site with medium or low engagement index that has accessed site does not
  // highlight UB and only shows the icon.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  page_specific_content_settings()->OnBrowsingDataAccessed(

      CreateUnpartitionedStorageKey(GURL("https://anotherthirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Revisiting high site engagement site doesn't highlight UB
  // because the entry point was already highlighted for that site.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  NavigateAndCommit(GURL("https://highengagement.com"));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, StorageAccessApiHighSiteEngagement) {
  base::HistogramTester t;
  // An engagement score above HIGH.
  const int kHighEngagement = 60;

  site_engagement::SiteEngagementService::Get(profile())->ResetBaseScoreForURL(
      GURL("https://highengagement.com"), kHighEngagement);

  // Create storage access exception for https://highengagement.com as top-level
  // origin.
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());
  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURL(GURL("https://thirdparty.com")),
      ContentSettingsPattern::FromURL(GURL("https://highengagement.com")),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  NavigateAndCommit(GURL("https://highengagement.com"));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Even though the site has high engagement level, UB does not highlight
  // because SAA was requested in the site context.
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  page_specific_content_settings()->OnBrowsingDataAccessed(

      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling third-party cookies records metrics.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           CookieControlsState::kAllowed3pc,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  t.ExpectUniqueSample(kCookieControlsActivatedSaaHistogram, true, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedRefreshCountHistogram, 0, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedSiteEngagementHistogram,
                       kHighEngagement, 1);
  t.ExpectUniqueSample(
      kCookieControlsActivatedSiteDataAccessHistogram,
      ThirdPartySiteDataAccessType::kAnyAllowedThirdPartySiteAccesses, 1);
  ValidateCookieControlsActivatedUKM(
      /*fed_cm_initiated=*/false,
      /*storage_access_api_requested=*/true,
      /*page_refresh_count=*/0, /*repeated_activation=*/false,
      blink::mojom::EngagementLevel::HIGH,
      ThirdPartySiteDataAccessType::kAnyAllowedThirdPartySiteAccesses);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, CustomExceptionsNoWildcardMatchingDomain) {
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());

  NavigateAndCommit(GURL("https://cool.things.com"));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Creating an exception turns protections off. The exception doesn't contain
  // wildcards in the domain and isn't enforced.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kAllowed3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("cool.things.com"),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, CustomExceptionsWildcardMatchingDomain) {
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());

  NavigateAndCommit(GURL("https://cool.things.com"));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Creating an exception turns protections off. The exception has wildcard in
  // the domain and cannot be reset, it is enforced by cookie setting.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kAllowed3pc,
                      CookieControlsEnforcement::kEnforcedByCookieSetting,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("[*.]cool.things.com"),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest,
       CustomExceptionsWildcardLessSpecificDomain) {
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());

  NavigateAndCommit(GURL("https://cool.things.com"));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Creating an exception changes turns protections off. The exception
  // has wildcard in the domain and cannot be reset, it is enforced by cookie
  // setting.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kAllowed3pc,
                      CookieControlsEnforcement::kEnforcedByCookieSetting,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));

  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("[*.]things.com"),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, CustomExceptionsDotComWildcard) {
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());

  NavigateAndCommit(GURL("https://cool.things.com"));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/false, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Creating an exception turns protections off. The exception
  // is set at the TLD level and cannot be reset, it is enforced by cookie
  // setting.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kAllowed3pc,
                      CookieControlsEnforcement::kEnforcedByCookieSetting,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("[*.]com"),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, FinishedPageReloadWithChangedSettings) {
  // Check that when the page is reloaded after settings have changed, that
  // the appropriate observer method is fired. Reloading the page without a
  // change, should not fire the observer.
  EXPECT_CALL(*mock(), OnFinishedPageReloadWithChangedSettings()).Times(0);
  cookie_controls()->Update(web_contents());
  NavigateAndCommit(GURL(kUrl));

  // Loading the same page after not making an effective change should not fire.
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  ValidateCookieControlsActivatedUKM(
      /*fed_cm_initiated=*/false,
      /*storage_access_api_requested=*/false,
      /*page_refresh_count=*/0, /*repeated_activation=*/false,
      blink::mojom::EngagementLevel::NONE,
      ThirdPartySiteDataAccessType::kNoThirdPartySiteAccesses);

  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  NavigateAndCommit(GURL(kUrl));

  // Loading a different page after making an effective change should not fire.
  cookie_controls()->SetUserChangedCookieBlockingForSite(true);
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  ValidateCookieControlsActivatedUKM(
      /*fed_cm_initiated=*/false,
      /*storage_access_api_requested=*/false,
      /*page_refresh_count=*/1, /*repeated_activation=*/true,
      blink::mojom::EngagementLevel::NONE,
      ThirdPartySiteDataAccessType::kNoThirdPartySiteAccesses);

  NavigateAndCommit(GURL("https://example2.com"));
  testing::Mock::VerifyAndClearExpectations(mock());

  // Observer should fire when reloaded after change.
  EXPECT_CALL(*mock(), OnFinishedPageReloadWithChangedSettings()).Times(2);
  cookie_controls()->SetUserChangedCookieBlockingForSite(true);
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  ValidateCookieControlsActivatedUKM(
      /*fed_cm_initiated=*/false,
      /*storage_access_api_requested=*/false,
      /*page_refresh_count=*/0, /*repeated_activation=*/false,
      blink::mojom::EngagementLevel::NONE,
      ThirdPartySiteDataAccessType::kNoThirdPartySiteAccesses);

  NavigateAndCommit(GURL("https://example2.com"));
  cookie_controls()->SetUserChangedCookieBlockingForSite(true);
  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  NavigateAndCommit(GURL("https://example2.com"));
}

TEST_F(CookieControlsUserBypassTest,
       DoesNotHighlightLabelWhenSettingNotChangedInContext) {
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());
  EXPECT_CALL(*mock(), OnFinishedPageReloadWithChangedSettings()).Times(0);
  cookie_controls()->Update(web_contents());
  NavigateAndCommit(GURL(kUrl));
  testing::Mock::VerifyAndClearExpectations(mock());

  EXPECT_CALL(*mock(), OnFinishedPageReloadWithChangedSettings()).Times(0);
  hcsm->SetContentSettingCustomScope(ContentSettingsPattern::Wildcard(),
                                     ContentSettingsPattern::FromString(kUrl),
                                     ContentSettingsType::COOKIES,
                                     CONTENT_SETTING_ALLOW);
  NavigateAndCommit(GURL(kUrl));
  testing::Mock::VerifyAndClearExpectations(mock());

  EXPECT_CALL(*mock(), OnFinishedPageReloadWithChangedSettings()).Times(0);
  hcsm->SetContentSettingCustomScope(ContentSettingsPattern::Wildcard(),
                                     ContentSettingsPattern::FromString(kUrl),
                                     ContentSettingsType::COOKIES,
                                     CONTENT_SETTING_BLOCK);
  NavigateAndCommit(GURL(kUrl));
  testing::Mock::VerifyAndClearExpectations(mock());

  EXPECT_CALL(*mock(), OnFinishedPageReloadWithChangedSettings()).Times(0);
  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("[*.]example.com"),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  NavigateAndCommit(GURL(kUrl));
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, IconHighlightedAfterExceptionExpires) {
  NavigateAndCommit(GURL(kUrl));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/true);

  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enable third-party cookies.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           CookieControlsState::kAllowed3pc,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  ValidateCookieControlsActivatedUKM(
      /*fed_cm_initiated=*/false,
      /*storage_access_api_requested=*/false,
      /*page_refresh_count=*/0, /*repeated_activation=*/false,
      blink::mojom::EngagementLevel::NONE,
      ThirdPartySiteDataAccessType::kAnyBlockedThirdPartySiteAccesses);

  NavigateAndCommit(GURL(kUrl));
  testing::Mock::VerifyAndClearExpectations(mock());

  // Wait for exception to expire.
  FastForwardTo(expiration() + base::Days(1));

  // Visiting the site after exception expires highlights UB.
  NavigateAndCommit(GURL(kUrl));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/true);
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/true));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Revisiting the site again after 30 seconds shouldn't highlight UB.
  FastForwardBy(base::Seconds(30));
  NavigateAndCommit(GURL(kUrl));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked=*/true);
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, StatefulBounce) {
  NavigateAndCommit(GURL(kUrl));
  page_specific_content_settings()->IncrementStatefulBounceCount();

  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
}

TEST_F(CookieControlsUserBypassTest, SubresourceBlocked) {
  base::test::ScopedFeatureList fingerprinting_protection_feature_list;
  fingerprinting_protection_feature_list.InitAndEnableFeature(
      fingerprinting_protection_filter::features::
          kEnableFingerprintingProtectionFilter);
  CreateFingerprintingProtectionWebContentsHelper(
      web_contents(), /*pref_service=*/nullptr, /*content_settings=*/nullptr,
      /*tracking_protection_settings=*/nullptr, /*is_incognito=*/false);

  NavigateAndCommit(GURL(kUrl));
  fingerprinting_protection_filter::FingerprintingProtectionWebContentsHelper::
      FromWebContents(web_contents())
          ->NotifyOnBlockedSubresource(
              subresource_filter::mojom::ActivationLevel::kEnabled);

  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
}

TEST_F(CookieControlsUserBypassTest, SubresourceBlockedInIncognito) {
  base::test::ScopedFeatureList fingerprinting_protection_feature_list;
  fingerprinting_protection_feature_list.InitAndEnableFeature(
      fingerprinting_protection_filter::features::
          kEnableFingerprintingProtectionFilterInIncognito);
  CreateFingerprintingProtectionWebContentsHelper(
      web_contents(), /*pref_service=*/nullptr, /*content_settings=*/nullptr,
      /*tracking_protection_settings=*/nullptr, /*is_incognito=*/true);

  NavigateAndCommit(GURL(kUrl));
  fingerprinting_protection_filter::FingerprintingProtectionWebContentsHelper::
      FromWebContents(web_contents())
          ->NotifyOnBlockedSubresource(
              subresource_filter::mojom::ActivationLevel::kEnabled);

  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
}

TEST_F(CookieControlsUserBypassTest, SubresourceProxied) {
  base::test::ScopedFeatureList ip_protection_feature_list;
  ip_protection_feature_list.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  ip_protection::IpProtectionStatus::CreateForWebContents(web_contents());

  NavigateAndCommit(GURL(kUrl));

  ip_protection::IpProtectionStatus::FromWebContents(web_contents())
      ->ResourceLoadComplete(ChromeRenderViewHostTestHarness::main_rfh(),
                             content::GlobalRequestID(),
                             *CreateResourceLoadInfoWithIpProtectionChain());

  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
}

TEST_F(CookieControlsUserBypassTest, SandboxedTopLevelFrame) {
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader("Content-Security-Policy", "sandbox");

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL(kUrl), web_contents());
  navigation->SetResponseHeaders(headers);
  navigation->Start();
  navigation->Commit();
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest,
       FrequentPageReloadsWithoutUpdateBeingCalled) {
  NavigateAndCommit(GURL(kUrl));
  // Call the entry point animated function without setting up the observer.
  cookie_controls()->OnEntryPointAnimated();
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());

  // A setting is not recorded.
  base::Value stored_value = hcsm->GetWebsiteSetting(
      GURL(kUrl), GURL(), ContentSettingsType::COOKIE_CONTROLS_METADATA);
  EXPECT_TRUE(stored_value.is_none());
  testing::Mock::VerifyAndClearExpectations(mock());
}
class CookieControlsUserBypassIncognitoTest
    : public CookieControlsUserBypassTest {
 public:
  CookieControlsUserBypassIncognitoTest() = default;
  ~CookieControlsUserBypassIncognitoTest() override = default;

  void SetUp() override {
    CookieControlsUserBypassTest::SetUp();

    incognito_cookie_controls_ =
        std::make_unique<content_settings::CookieControlsController>(
            CookieSettingsFactory::GetForProfile(incognito_profile()),
            CookieSettingsFactory::GetForProfile(profile()),
            HostContentSettingsMapFactory::GetForProfile(incognito_profile()),
            TrackingProtectionSettingsFactory::GetForProfile(
                incognito_profile()),
            /*is_incognito_profile=*/true);

    incognito_cookie_controls_->AddObserver(mock());
  }

  void TearDown() override {
    incognito_cookie_controls_->RemoveObserver(mock());
    incognito_cookie_controls_.reset();
    CookieControlsUserBypassTest::TearDown();
  }

  content_settings::CookieControlsController* incognito_cookie_controls() {
    return incognito_cookie_controls_.get();
  }

  Profile* incognito_profile() {
    return profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

 private:
  std::unique_ptr<content_settings::CookieControlsController>
      incognito_cookie_controls_;
};

TEST_F(CookieControlsUserBypassIncognitoTest, AddTrackingProtectionException) {
  NavigateAndCommit(GURL(kUrl));
  privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings =
      TrackingProtectionSettingsFactory::GetForProfile(incognito_profile());

  incognito_cookie_controls()->Update(web_contents());
  incognito_cookie_controls()->OnTrackingProtectionsChangedForSite(true);

  EXPECT_TRUE(
      tracking_protection_settings->HasTrackingProtectionException(GURL(kUrl)));
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassIncognitoTest,
       RemoveTrackingProtectionException) {
  NavigateAndCommit(GURL(kUrl));
  privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings =
      TrackingProtectionSettingsFactory::GetForProfile(incognito_profile());
  incognito_cookie_controls()->Update(web_contents());
  // Ensure tracking protection exception is created.
  tracking_protection_settings->AddTrackingProtectionException(GURL(kUrl));
  EXPECT_TRUE(
      tracking_protection_settings->HasTrackingProtectionException(GURL(kUrl)));

  incognito_cookie_controls()->OnTrackingProtectionsChangedForSite(false);

  EXPECT_FALSE(
      tracking_protection_settings->HasTrackingProtectionException(GURL(kUrl)));

  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassIncognitoTest, ToggleUpdatesUi) {
  incognito_cookie_controls()->Update(web_contents());
  NavigateAndCommit(GURL(kUrl));

  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kAllowed3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));

  incognito_cookie_controls()->OnCookieBlockingEnabledForSite(false);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassIncognitoTest, SubresourceProxied) {
  base::test::ScopedFeatureList ip_protection_feature_list;
  ip_protection_feature_list.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  ip_protection::IpProtectionStatus::CreateForWebContents(web_contents());

  NavigateAndCommit(GURL(kUrl));

  ip_protection::IpProtectionStatus::FromWebContents(web_contents())
      ->ResourceLoadComplete(ChromeRenderViewHostTestHarness::main_rfh(),
                             content::GlobalRequestID(),
                             *CreateResourceLoadInfoWithIpProtectionChain());

  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsState::kBlocked3pc,
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kBlocked3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));

  incognito_cookie_controls()->Update(web_contents());
}

const char kUMAFppActiveDisableProtections[] =
    "TrackingProtections.Bubble.FppActive.DisableProtections";
const char kUMAFppActiveEnableProtections[] =
    "TrackingProtections.Bubble.FppActive.EnableProtections";
const char kUMAIppActiveDisableProtections[] =
    "TrackingProtections.Bubble.IppActive.DisableProtections";
const char kUMAIppActiveEnableProtections[] =
    "TrackingProtections.Bubble.IppActive.EnableProtections";

enum class ActFeatureState {
  kActFeaturesEnabled = 0,
  kFppDisabled = 1,
  kIppDisabled = 2,
};

class CookieControlsUserBypassTrackingProtectionUiTest
    : public CookieControlsUserBypassTest,
      public testing::WithParamInterface<
          testing::tuple<CookieControlsState, ActFeatureState>> {
 public:
  CookieControlsUserBypassTrackingProtectionUiTest() = default;
  ~CookieControlsUserBypassTrackingProtectionUiTest() override = default;

  void TearDown() override {
    incognito_cookie_controls_->RemoveObserver(incognito_mock());
    incognito_cookie_controls_.reset();
    incognito_web_contents_ = nullptr;
    incognito_profile_ = nullptr;
    CookieControlsUserBypassTest::TearDown();
  }

  void SetUp() override {
    CookieControlsUserBypassTest::SetUp();

    std::vector<base::test::FeatureRef> enabled_features = {
        privacy_sandbox::kActUserBypassUx};
    if (std::get<1>(GetParam()) != ActFeatureState::kIppDisabled) {
      enabled_features.push_back(privacy_sandbox::kIpProtectionUx);
      enabled_features.push_back(net::features::kEnableIpProtectionProxy);
      profile()->GetPrefs()->SetBoolean(prefs::kIpProtectionEnabled, true);
    }
    if (std::get<1>(GetParam()) != ActFeatureState::kFppDisabled) {
      enabled_features.push_back(privacy_sandbox::kFingerprintingProtectionUx);
      enabled_features.push_back(fingerprinting_protection_filter::features::
                                     kEnableFingerprintingProtectionFilter);
      profile()->GetPrefs()->SetBoolean(prefs::kFingerprintingProtectionEnabled,
                                        true);
    }
    feature_list_.InitWithFeatures(enabled_features, {});

    incognito_profile_ =
        profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    incognito_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        incognito_profile_, nullptr);
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        incognito_web_contents_.get(),
        std::make_unique<PageSpecificContentSettingsDelegate>(
            incognito_web_contents_.get()));

    incognito_cookie_controls_ =
        std::make_unique<content_settings::CookieControlsController>(
            CookieSettingsFactory::GetForProfile(incognito_profile_),
            CookieSettingsFactory::GetForProfile(profile()),
            HostContentSettingsMapFactory::GetForProfile(incognito_profile_),
            TrackingProtectionSettingsFactory::GetForProfile(
                incognito_profile_),
            /*is_incognito_profile=*/true);
    incognito_cookie_controls_->AddObserver(incognito_mock());
  }

  void ProxyIpSubresource() {
    ip_protection::IpProtectionStatus::FromWebContents(web_contents())
        ->ResourceLoadComplete(ChromeRenderViewHostTestHarness::main_rfh(),
                               content::GlobalRequestID(),
                               *CreateResourceLoadInfoWithIpProtectionChain());
  }

  void BlockFingerprintingSubresource() {
    fingerprinting_protection_filter::
        FingerprintingProtectionWebContentsHelper::FromWebContents(
            web_contents())
            ->NotifyOnBlockedSubresource(
                subresource_filter::mojom::ActivationLevel::kEnabled);
  }

  void AddSiteException() {
    scoped_refptr<content_settings::CookieSettings> cookie_settings =
        CookieSettingsFactory::GetForProfile(incognito_profile());
    cookie_settings->SetThirdPartyCookieSetting(
        GURL(kUrl), ContentSetting::CONTENT_SETTING_ALLOW);
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings =
        TrackingProtectionSettingsFactory::GetForProfile(incognito_profile());
    tracking_protection_settings->AddTrackingProtectionException(GURL(kUrl));
  }

  content::WebContents* incognito_web_contents() {
    return incognito_web_contents_.get();
  }

  Profile* incognito_profile() { return incognito_profile_.get(); }

  MockCookieControlsObserver* incognito_mock() { return &incognito_mock_; }

  content_settings::CookieControlsController* incognito_cookie_controls() {
    return incognito_cookie_controls_.get();
  }

 protected:
  base::UserActionTester user_actions_;

 private:
  base::test::ScopedFeatureList feature_list_;
  MockCookieControlsObserver incognito_mock_;
  raw_ptr<Profile> incognito_profile_;
  std::unique_ptr<content::WebContents> incognito_web_contents_;
  std::unique_ptr<content_settings::CookieControlsController>
      incognito_cookie_controls_;
};

TEST_P(CookieControlsUserBypassTrackingProtectionUiTest,
       PauseOrAllowTrackingProtectionsBasedOnCookieControlsState) {
  auto* tester = content::WebContentsTester::For(incognito_web_contents());
  tester->NavigateAndCommit(GURL(kUrl));
  incognito_cookie_controls()->Update(incognito_web_contents());
  incognito_cookie_controls()->OnTrackingProtectionsChangedForSite(
      std::get<0>(GetParam()) == CookieControlsState::kPausedTp);

  EXPECT_CALL(
      *incognito_mock(),
      OnStatusChanged(std::get<0>(GetParam()),
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  incognito_cookie_controls()->Update(incognito_web_contents());
  testing::Mock::VerifyAndClearExpectations(incognito_mock());
}

TEST_P(CookieControlsUserBypassTrackingProtectionUiTest,
       ProtectionsOnForActFeaturesWhenCookiesAreEnforced) {
  NavigateAndCommit(GURL(kUrl));
  cookie_controls()->Update(web_contents());
  auto* tester = content::WebContentsTester::For(incognito_web_contents());
  tester->NavigateAndCommit(GURL(kUrl));
  incognito_cookie_controls()->Update(incognito_web_contents());

  bool tp_paused = std::get<0>(GetParam()) == CookieControlsState::kPausedTp;

  incognito_cookie_controls()->OnTrackingProtectionsChangedForSite(tp_paused);

  // Allowing 3PCs in regular mode should allow & enforce them in incognito.
  // Protections (i.e. the toggle) should still be on iff ACT features are
  // enabled.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           CookieControlsState::kAllowed3pc,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration()));
  EXPECT_CALL(*mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, CookieControlsState::kAllowed3pc,
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));

  EXPECT_CALL(
      *incognito_mock(),
      OnStatusChanged(std::get<0>(GetParam()),
                      CookieControlsEnforcement::kEnforcedByCookieSetting,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));

  EXPECT_CALL(*incognito_mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/
                  tp_paused,  // Icon shown when protections paused
                  std::get<0>(GetParam()), CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(incognito_mock());
}

TEST_P(CookieControlsUserBypassTrackingProtectionUiTest,
       RecordUmaToggleMetricWhenActFeaturesAreActive) {
  bool tp_paused = std::get<0>(GetParam()) == CookieControlsState::kPausedTp;
  bool ipp_enabled = std::get<1>(GetParam()) != ActFeatureState::kIppDisabled;
  bool fpp_enabled = std::get<1>(GetParam()) != ActFeatureState::kFppDisabled;
  incognito_cookie_controls()->Update(web_contents());

  // Add site exception when protections are on so toggling UB initiates the
  // observer calls correctly.
  if (!tp_paused) {
    AddSiteException();
  }

  // Set up IPP and FPP for blocking / proxying.
  if (ipp_enabled) {
    ip_protection::IpProtectionStatus::CreateForWebContents(web_contents());
  }
  if (fpp_enabled) {
    CreateFingerprintingProtectionWebContentsHelper(
        web_contents(), /*pref_service=*/nullptr, /*content_settings=*/nullptr,
        /*tracking_protection_settings=*/nullptr, /*is_incognito=*/false);
  }

  NavigateAndCommit(GURL(kUrl));

  if (ipp_enabled) {
    ProxyIpSubresource();
  }
  if (fpp_enabled) {
    BlockFingerprintingSubresource();
  }

  EXPECT_CALL(
      *incognito_mock(),
      OnStatusChanged(std::get<0>(GetParam()),
                      CookieControlsEnforcement::kNoEnforcement,
                      CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration()));
  EXPECT_CALL(*incognito_mock(),
              OnCookieControlsIconStatusChanged(
                  /*icon_visible=*/true, std::get<0>(GetParam()),
                  CookieBlocking3pcdStatus::kNotIn3pcd,
                  /*should_highlight=*/false));

  incognito_cookie_controls()->OnTrackingProtectionsChangedForSite(tp_paused);

  EXPECT_EQ(user_actions_.GetActionCount(kUMAIppActiveEnableProtections),
            ipp_enabled && !tp_paused ? 1 : 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAIppActiveDisableProtections),
            ipp_enabled && tp_paused ? 1 : 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAFppActiveEnableProtections),
            fpp_enabled && !tp_paused ? 1 : 0);
  EXPECT_EQ(user_actions_.GetActionCount(kUMAFppActiveDisableProtections),
            fpp_enabled && tp_paused ? 1 : 0);
}

std::string ParamToTestSuffixTrackingProtection(
    const testing::TestParamInfo<
        CookieControlsUserBypassTrackingProtectionUiTest::ParamType>& info) {
  std::stringstream name;
  if (std::get<0>(info.param) == CookieControlsState::kActiveTp) {
    name << "TpActive";
  } else {
    name << "TpPaused";
  }
  switch (std::get<1>(info.param)) {
    case ActFeatureState::kActFeaturesEnabled:
      name << "_kActFeaturesEnabled";
      break;
    case ActFeatureState::kIppDisabled:
      name << "_kIppDisabled";
      break;
    case ActFeatureState::kFppDisabled:
      name << "_kFppDisabled";
      break;
  }

  return name.str();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CookieControlsUserBypassTrackingProtectionUiTest,
    testing::Combine(
        /*controls_state*/ testing::Values(CookieControlsState::kActiveTp,
                                           CookieControlsState::kPausedTp),
        testing::Values(ActFeatureState::kActFeaturesEnabled,
                        ActFeatureState::kIppDisabled,
                        ActFeatureState::kFppDisabled)),
    &ParamToTestSuffixTrackingProtection);
