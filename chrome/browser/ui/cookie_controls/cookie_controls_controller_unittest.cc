// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/ui/cookie_controls_controller.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
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
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/common/third_party_site_data_access_type.h"
#include "components/content_settings/core/common/tracking_protection_feature.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using StorageType =
    content_settings::mojom::ContentSettingsManager::StorageType;
using BlockingStatus = content_settings::TrackingProtectionBlockingStatus;
using FeatureType = content_settings::TrackingProtectionFeatureType;
using TrackingProtectionFeature = content_settings::TrackingProtectionFeature;

constexpr char kCookieControlsActivatedSaaHistogram[] =
    "Privacy.CookieControlsActivated.SaaRequested";
constexpr char kCookieControlsActivatedRefreshCountHistogram[] =
    "Privacy.CookieControlsActivated.PageRefreshCount";
constexpr char kCookieControlsActivatedSiteEngagementHistogram[] =
    "Privacy.CookieControlsActivated.SiteEngagementScore";
constexpr char kCookieControlsActivatedSiteDataAccessHistogram[] =
    "Privacy.CookieControlsActivated.SiteDataAccessType";

class MockCookieControlsObserver
    : public content_settings::CookieControlsObserver {
 public:
  MOCK_METHOD(void,
              OnStatusChanged,
              (/*controls_visible*/ bool,
               /*protections_on*/ bool,
               CookieControlsEnforcement,
               CookieBlocking3pcdStatus,
               base::Time,
               std::vector<TrackingProtectionFeature>));
  MOCK_METHOD(void,
              OnCookieControlsIconStatusChanged,
              (/*icon_visible*/ bool,
               /*protections_on*/ bool,
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
            tracking_protection_settings_);
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

  std::vector<TrackingProtectionFeature>
  GetThirdPartyCookiesFeatureForEnforcement(
      CookieControlsEnforcement enforcement,
      BlockingStatus blocking_status) {
    return {{FeatureType::kThirdPartyCookies, enforcement, blocking_status}};
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
  NavigateAndCommit(GURL("https://example.com"));
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
  NavigateAndCommit(GURL("https://example.com"));
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
  NavigateAndCommit(GURL("https://example.com"));

  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should be notified.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://anotherthirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/true);

  // Enabling third-party cookies records metrics.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           /*controls_visible=*/true, /*protections_on=*/false,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration(),
                           GetThirdPartyCookiesFeatureForEnforcement(
                               CookieControlsEnforcement::kNoEnforcement,
                               BlockingStatus::kAllowed)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/false,
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
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/false, /*protections_on=*/false,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kAllowed)));
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/false,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
}

TEST_F(CookieControlsUserBypassTest, PreferenceDisabled) {
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling the feature should disable the UI.
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/false, /*protections_on=*/false,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kAllowed)));
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/false,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));
  testing::Mock::VerifyAndClearExpectations(mock());
}
TEST_F(CookieControlsUserBypassTest, AllCookiesBlocked) {
  base::HistogramTester t;
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disable all cookies - an OnStatusCallback should get triggered.
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disable cookie blocking for example.com.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           /*controls_visible=*/true, /*protections_on=*/false,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration(),
                           GetThirdPartyCookiesFeatureForEnforcement(
                               CookieControlsEnforcement::kNoEnforcement,
                               BlockingStatus::kAllowed)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/false,
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
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling cookie blocking for example.com should update the ui.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           /*controls_visible=*/true, /*protections_on=*/false,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration(),
                           GetThirdPartyCookiesFeatureForEnforcement(
                               CookieControlsEnforcement::kNoEnforcement,
                               BlockingStatus::kAllowed)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/false,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Visiting some other site, should re-enable protections.
  NavigateAndCommit(GURL("https://somethingelse.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Visiting example.com should turn protections off.
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(), OnStatusChanged(
                           /*controls_visible=*/true, /*protections_on=*/false,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration(),
                           GetThirdPartyCookiesFeatureForEnforcement(
                               CookieControlsEnforcement::kNoEnforcement,
                               BlockingStatus::kAllowed)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/false,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling example.com again should re-enable protections.
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, Incognito) {
  NavigateAndCommit(GURL("https://example.com"));

  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Create incognito web_contents and
  // content_settings::CookieControlsController.
  std::unique_ptr<content::WebContents> incognito_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), nullptr);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      incognito_web_contents.get(),
      std::make_unique<PageSpecificContentSettingsDelegate>(
          incognito_web_contents.get()));
  auto* tester = content::WebContentsTester::For(incognito_web_contents.get());
  MockCookieControlsObserver incognito_mock_;
  content_settings::CookieControlsController incognito_cookie_controls(
      CookieSettingsFactory::GetForProfile(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)),
      CookieSettingsFactory::GetForProfile(profile()),
      HostContentSettingsMapFactory::GetForProfile(profile()),
      TrackingProtectionSettingsFactory::GetForProfile(profile()));
  incognito_cookie_controls.AddObserver(&incognito_mock_);

  // Navigate incognito web_contents to the same URL.
  tester->NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(incognito_mock_,
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(
      incognito_mock_,
      OnCookieControlsIconStatusChanged(
          /*icon_visible=*/false, /*protections_on=*/true,
          CookieBlocking3pcdStatus::kNotIn3pcd, /*should_highlight=*/false));
  incognito_cookie_controls.Update(incognito_web_contents.get());
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(&incognito_mock_);

  // Allow cookies in regular mode should also allow in incognito but enforced
  // through regular mode.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           /*controls_visible=*/true, /*protections_on=*/false,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration(),
                           GetThirdPartyCookiesFeatureForEnforcement(
                               CookieControlsEnforcement::kNoEnforcement,
                               BlockingStatus::kAllowed)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/false,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));

  EXPECT_CALL(incognito_mock_,
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/false,
                  CookieControlsEnforcement::kEnforcedByCookieSetting,
                  CookieBlocking3pcdStatus::kNotIn3pcd, expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kEnforcedByCookieSetting,
                      BlockingStatus::kAllowed)));

  EXPECT_CALL(
      incognito_mock_,
      OnCookieControlsIconStatusChanged(
          /*icon_visible=*/true, /*protections_on=*/false,
          CookieBlocking3pcdStatus::kNotIn3pcd, /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(&incognito_mock_);

  // This should be enforced regardless of the default cookie setting in the
  // default profile.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           /*controls_visible=*/false, /*protections_on=*/false,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           // Although there is an allow exception with an
                           // expiration, because the default allow never
                           // expires, zero_expiration is correct.
                           zero_expiration(),
                           GetThirdPartyCookiesFeatureForEnforcement(
                               CookieControlsEnforcement::kNoEnforcement,
                               BlockingStatus::kAllowed)));
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/false,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));

  EXPECT_CALL(incognito_mock_,
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/false,
                  CookieControlsEnforcement::kEnforcedByCookieSetting,
                  CookieBlocking3pcdStatus::kNotIn3pcd, expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kEnforcedByCookieSetting,
                      BlockingStatus::kAllowed)));

  EXPECT_CALL(
      incognito_mock_,
      OnCookieControlsIconStatusChanged(
          /*icon_visible=*/true, /*protections_on=*/false,
          CookieBlocking3pcdStatus::kNotIn3pcd, /*should_highlight=*/false));
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kIncognitoOnly));
  incognito_cookie_controls.Update(incognito_web_contents.get());
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(&incognito_mock_);
}

TEST_F(CookieControlsUserBypassTest, ThirdPartyCookiesException) {
  // Create third party cookies exception.
  cookie_settings()->SetThirdPartyCookieSetting(
      GURL("https://example.com"), ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateAndCommit(GURL("https://example.com"));
  // Third-party cookie exceptions are handled in the same way as exceptions
  // created for user bypass.
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/false,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kAllowed)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/false,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling 3PC for example.com again should change status to kEnabled.
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, FrequentPageReloads) {
  // Update on the initial web contents to ensure the tab observer is setup.
  cookie_controls()->Update(web_contents());
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());

  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  NavigateAndCommit(GURL("https://example.com"));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should be notified.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  page_specific_content_settings()->OnBrowsingDataAccessed(

      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Reload the page and simulate accessing storage on page load.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // After the second reload and accessing storage, UB should highlight.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/true));

  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/false);
  cookie_controls()->OnEntryPointAnimated();
  testing::Mock::VerifyAndClearExpectations(mock());

  // After the entry point was highlighted, a setting is recorded.
  base::Value stored_value =
      hcsm->GetWebsiteSetting(GURL("https://example.com"), GURL(),
                              ContentSettingsType::COOKIE_CONTROLS_METADATA);
  EXPECT_TRUE(stored_value.is_dict());
  EXPECT_TRUE(stored_value.GetDict().FindBool("entry_point_animated").value());
}

TEST_F(CookieControlsUserBypassTest,
       HittingPageReloadThresholdTriggersOnReloadThresholdExceeded) {
  // Update initial web contents to ensure the tab observer is set up.
  cookie_controls()->Update(web_contents());

  // Don't call observer when reload count = 0.
  EXPECT_CALL(*mock(), OnReloadThresholdExceeded()).Times(0);

  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  NavigateAndCommit(GURL("https://example.com"));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Don't call observer when reload count = 1.
  EXPECT_CALL(*mock(), OnReloadThresholdExceeded()).Times(0);

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  NavigateAndCommit(GURL("https://example.com"));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Expect observer call when reload count hits threshold of 2.
  EXPECT_CALL(*mock(), OnReloadThresholdExceeded());
  // Expect that we attempt to highlight the user bypass icon.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/true));
  NavigateAndCommit(GURL("https://example.com"));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, FrequentPageReloadsMetrics) {
  base::HistogramTester t;
  cookie_controls()->Update(web_contents());

  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should be notified.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Reload the page and simulate accessing storage on page load.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // After the second reload and accessing storage, UB should be highlighted.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/true));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling third-party cookies records metrics.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           /*controls_visible=*/true, /*protections_on=*/false,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration(),
                           GetThirdPartyCookiesFeatureForEnforcement(
                               CookieControlsEnforcement::kNoEnforcement,
                               BlockingStatus::kAllowed)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/false,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/true));
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
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should be notified.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  page_specific_content_settings()->OnBrowsingDataAccessed(

      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Reload the page and simulate accessing storage on page load.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Wait for 30 seconds.
  FastForwardBy(base::Seconds(30));

  // The second reload happens with a delay and doesn't highlight.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling third-party cookies records metrics.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           /*controls_visible=*/true, /*protections_on=*/false,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration(),
                           GetThirdPartyCookiesFeatureForEnforcement(
                               CookieControlsEnforcement::kNoEnforcement,
                               BlockingStatus::kAllowed)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/false,
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
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/true));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Site data access should highlight.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/true));
  page_specific_content_settings()->OnBrowsingDataAccessed(

      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/false);
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
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Site with medium or low engagement index that has accessed site does not
  // highlight UB and only shows the icon.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  page_specific_content_settings()->OnBrowsingDataAccessed(

      CreateUnpartitionedStorageKey(GURL("https://anotherthirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Revisiting high site engagement site doesn't highlight UB
  // because the entry point was already highlighted for that site.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  NavigateAndCommit(GURL("https://highengagement.com"));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/false);
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
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Even though the site has high engagement level, UB does not highlight
  // because SAA was requested in the site context.
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  page_specific_content_settings()->OnBrowsingDataAccessed(

      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling third-party cookies records metrics.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           /*controls_visible=*/true, /*protections_on=*/false,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration(),
                           GetThirdPartyCookiesFeatureForEnforcement(
                               CookieControlsEnforcement::kNoEnforcement,
                               BlockingStatus::kAllowed)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/false,
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
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Creating an exception turns protections off. The exception doesn't contain
  // wildcards in the domain and isn't enforced.
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/false,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kAllowed)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/false,
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
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Creating an exception turns protections off. The exception has wildcard in
  // the domain and cannot be reset, it is enforced by cookie setting.
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/false,
                  CookieControlsEnforcement::kEnforcedByCookieSetting,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kEnforcedByCookieSetting,
                      BlockingStatus::kAllowed)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/false,
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
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Creating an exception changes turns protections off. The exception
  // has wildcard in the domain and cannot be reset, it is enforced by cookie
  // setting.
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/false,
                  CookieControlsEnforcement::kEnforcedByCookieSetting,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kEnforcedByCookieSetting,
                      BlockingStatus::kAllowed)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/false,
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
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Creating an exception turns protections off. The exception
  // is set at the TLD level and cannot be reset, it is enforced by cookie
  // setting.
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/false,
                  CookieControlsEnforcement::kEnforcedByCookieSetting,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kEnforcedByCookieSetting,
                      BlockingStatus::kAllowed)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/false,
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
  NavigateAndCommit(GURL("https://example.com"));

  // Loading the same page after not making an effective change should not fire.
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  ValidateCookieControlsActivatedUKM(
      /*fed_cm_initiated=*/false,
      /*storage_access_api_requested=*/false,
      /*page_refresh_count=*/0, /*repeated_activation=*/false,
      blink::mojom::EngagementLevel::NONE,
      ThirdPartySiteDataAccessType::kNoThirdPartySiteAccesses);

  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  NavigateAndCommit(GURL("https://example.com"));

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
  NavigateAndCommit(GURL("https://example.com"));
  testing::Mock::VerifyAndClearExpectations(mock());

  EXPECT_CALL(*mock(), OnFinishedPageReloadWithChangedSettings()).Times(0);
  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("https://example.com"),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  NavigateAndCommit(GURL("https://example.com"));
  testing::Mock::VerifyAndClearExpectations(mock());

  EXPECT_CALL(*mock(), OnFinishedPageReloadWithChangedSettings()).Times(0);
  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("https://example.com"),
      ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  NavigateAndCommit(GURL("https://example.com"));
  testing::Mock::VerifyAndClearExpectations(mock());

  EXPECT_CALL(*mock(), OnFinishedPageReloadWithChangedSettings()).Times(0);
  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("[*.]example.com"),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  NavigateAndCommit(GURL("https://example.com"));
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, IconHighlightedAfterExceptionExpires) {
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/true);

  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enable third-party cookies.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           /*controls_visible=*/true, /*protections_on=*/false,
                           CookieControlsEnforcement::kNoEnforcement,
                           CookieBlocking3pcdStatus::kNotIn3pcd, expiration(),
                           GetThirdPartyCookiesFeatureForEnforcement(
                               CookieControlsEnforcement::kNoEnforcement,
                               BlockingStatus::kAllowed)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/false,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  ValidateCookieControlsActivatedUKM(
      /*fed_cm_initiated=*/false,
      /*storage_access_api_requested=*/false,
      /*page_refresh_count=*/0, /*repeated_activation=*/false,
      blink::mojom::EngagementLevel::NONE,
      ThirdPartySiteDataAccessType::kAnyBlockedThirdPartySiteAccesses);

  NavigateAndCommit(GURL("https://example.com"));
  testing::Mock::VerifyAndClearExpectations(mock());

  // Wait for exception to expire.
  FastForwardTo(expiration() + base::Days(1));

  // Visiting the site after exception expires highlights UB.
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/true);
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/true));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Revisiting the site again after 30 seconds shouldn't highlight UB.
  FastForwardBy(base::Seconds(30));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnBrowsingDataAccessed(
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      BrowsingDataModel::StorageType::kQuotaStorage,
      /*blocked_by_policy=*/true);
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest, StatefulBounce) {
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->IncrementStatefulBounceCount();

  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
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
      web_contents(), /*pref_service=*/nullptr,
      /*tracking_protection_settings=*/nullptr, /*is_incognito=*/false);

  NavigateAndCommit(GURL("https://example.com"));
  fingerprinting_protection_filter::FingerprintingProtectionWebContentsHelper::
      FromWebContents(web_contents())
          ->NotifyOnBlockedResources();

  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
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
      web_contents(), /*pref_service=*/nullptr,
      /*tracking_protection_settings=*/nullptr, /*is_incognito=*/true);

  NavigateAndCommit(GURL("https://example.com"));
  fingerprinting_protection_filter::FingerprintingProtectionWebContentsHelper::
      FromWebContents(web_contents())
          ->NotifyOnBlockedResources();

  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kBlocked)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
}

TEST_F(CookieControlsUserBypassTest, SandboxedTopLevelFrame) {
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
  headers->SetHeader("Content-Security-Policy", "sandbox");

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com"), web_contents());
  navigation->SetResponseHeaders(headers);
  navigation->Start();
  navigation->Commit();
  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/true, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kNotIn3pcd,
                           /*should_highlight=*/false));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest,
       FrequentPageReloadsWithoutUpdateBeingCalled) {
  NavigateAndCommit(GURL("https://example.com"));
  // Call the entry point animated function without setting up the observer.
  cookie_controls()->OnEntryPointAnimated();
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());

  // A setting is not recorded.
  base::Value stored_value =
      hcsm->GetWebsiteSetting(GURL("https://example.com"), GURL(),
                              ContentSettingsType::COOKIE_CONTROLS_METADATA);
  EXPECT_TRUE(stored_value.is_none());
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsUserBypassTest,
       BlockingStatusLimitedWhenCookieControlsModePrefIsLimited) {
  NavigateAndCommit(GURL("https://cool.things.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, /*protections_on=*/true,
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kLimited, zero_expiration(),
                  GetThirdPartyCookiesFeatureForEnforcement(
                      CookieControlsEnforcement::kNoEnforcement,
                      BlockingStatus::kLimited)));

  EXPECT_CALL(*mock(), OnCookieControlsIconStatusChanged(
                           /*icon_visible=*/false, /*protections_on=*/true,
                           CookieBlocking3pcdStatus::kLimited,
                           /*should_highlight=*/false));
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kLimited));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());
}

class CookieControlsUserBypassTrackingProtectionUiTest
    : public CookieControlsUserBypassTest,
      public testing::WithParamInterface<testing::tuple<bool, bool, bool>> {
 public:
  CookieControlsUserBypassTrackingProtectionUiTest() = default;
  ~CookieControlsUserBypassTrackingProtectionUiTest() override = default;

  void SetUp() override {
    CookieControlsUserBypassTest::SetUp();
    if (std::get<0>(GetParam())) {
      cookie_settings()->SetThirdPartyCookieSetting(
          GURL("https://example.com"), ContentSetting::CONTENT_SETTING_BLOCK);
    } else {
      tracking_protection_settings()->AddTrackingProtectionException(
          GURL("https://example.com"));
      cookie_settings()->SetThirdPartyCookieSetting(
          GURL("https://example.com"), ContentSetting::CONTENT_SETTING_ALLOW);
    }

    std::vector<base::test::FeatureRef> enabled_features = {};
    if (std::get<1>(GetParam())) {
      enabled_features.push_back(privacy_sandbox::kIpProtectionUserBypass);
      enabled_features.push_back(privacy_sandbox::kIpProtectionV1);
      profile()->GetPrefs()->SetBoolean(prefs::kIpProtectionEnabled, true);
    }
    if (std::get<2>(GetParam())) {
      enabled_features.push_back(
          privacy_sandbox::kFingerprintingProtectionUserBypass);
    }
    feature_list_.InitWithFeatures(enabled_features, {});
  }

  std::vector<TrackingProtectionFeature> GetFeatureVector(
      CookieControlsEnforcement enforcement) {
    bool protections_on = std::get<0>(GetParam());
    std::vector<TrackingProtectionFeature> features_list;
    features_list.push_back(
        {FeatureType::kThirdPartyCookies, enforcement,
         protections_on ? BlockingStatus::kBlocked : BlockingStatus::kAllowed});
    // Currently these ACT features do not support different enforcement types.
    if (std::get<1>(GetParam())) {
      features_list.push_back({FeatureType::kIpProtection,
                               CookieControlsEnforcement::kNoEnforcement,
                               protections_on ? BlockingStatus::kHidden
                                              : BlockingStatus::kVisible});
    }
    if (std::get<2>(GetParam())) {
      features_list.push_back({FeatureType::kFingerprintingProtection,
                               CookieControlsEnforcement::kNoEnforcement,
                               protections_on ? BlockingStatus::kLimited
                                              : BlockingStatus::kAllowed});
    }
    return features_list;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(CookieControlsUserBypassTrackingProtectionUiTest,
       AddsActFeaturesToVectorBasedOnFeatureAndExceptionStatus) {
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(
                  /*controls_visible=*/true, std::get<0>(GetParam()),
                  CookieControlsEnforcement::kNoEnforcement,
                  CookieBlocking3pcdStatus::kNotIn3pcd, zero_expiration(),
                  GetFeatureVector(CookieControlsEnforcement::kNoEnforcement)));

  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CookieControlsUserBypassTrackingProtectionUiTest,
    testing::Combine(/*protections_on*/ testing::Bool(),
                     /*kIpProtectionUserBypass*/ testing::Bool(),
                     /*kFingerprintingProtectionUserBypass*/ testing::Bool()));
