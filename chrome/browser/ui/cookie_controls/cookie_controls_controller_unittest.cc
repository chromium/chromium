// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/ui/cookie_controls_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/browser/ui/cookie_controls_view.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/cookie_controls_breakage_confidence_level.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/content_settings/core/common/third_party_site_data_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class MockOldCookieControlsObserver
    : public content_settings::OldCookieControlsObserver {
 public:
  MOCK_METHOD(void,
              OnStatusChanged,
              (CookieControlsStatus, CookieControlsEnforcement, int, int));
  MOCK_METHOD(void, OnCookiesCountChanged, (int, int));
  MOCK_METHOD(void, OnStatefulBounceCountChanged, (int));
};

class MockCookieControlsObserver
    : public content_settings::CookieControlsObserver {
 public:
  MOCK_METHOD(void,
              OnStatusChanged,
              (CookieControlsStatus, CookieControlsEnforcement, base::Time));
  MOCK_METHOD(void, OnSitesCountChanged, (int, int));
  MOCK_METHOD(void,
              OnBreakageConfidenceLevelChanged,
              (CookieControlsBreakageConfidenceLevel));
  MOCK_METHOD(void, OnFinishedPageReloadWithChangedSettings, ());
};

blink::StorageKey CreateUnpartitionedStorageKey(const GURL& url) {
  return blink::StorageKey::CreateFirstParty(url::Origin::Create(url));
}

}  // namespace

// More readable output for test expectation.
std::ostream& operator<<(std::ostream& os, const CookieControlsStatus& status) {
  switch (status) {
    case CookieControlsStatus::kDisabled:
      return os << "kDisabled";
    case CookieControlsStatus::kEnabled:
      return os << "kEnabled";
    case CookieControlsStatus::kDisabledForSite:
      return os << "kDisabledForSite";
    case CookieControlsStatus::kUninitialized:
      return os << "kUninitialized";
  }
}

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
  }
}

class CookieControlsTest : public ChromeRenderViewHostTestHarness {
 public:
  CookieControlsTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
            web_contents()));
    profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
    NavigateAndCommit(GURL("chrome://newtab"));

    cookie_settings_ = CookieSettingsFactory::GetForProfile(profile());
    cookie_controls_ =
        std::make_unique<content_settings::CookieControlsController>(
            cookie_settings_, nullptr,
            HostContentSettingsMapFactory::GetForProfile(profile()));
    cookie_controls_->AddObserver(mock());
    testing::Mock::VerifyAndClearExpectations(mock());
  }

  void TearDown() override {
    cookie_controls_->RemoveObserver(mock());
    cookie_controls_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content_settings::CookieControlsController* cookie_controls() {
    return cookie_controls_.get();
  }

  content_settings::CookieSettings* cookie_settings() {
    return cookie_settings_.get();
  }

  MockOldCookieControlsObserver* mock() { return &mock_; }

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

 private:
  MockOldCookieControlsObserver mock_;
  std::unique_ptr<content_settings::CookieControlsController> cookie_controls_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
};

TEST_F(CookieControlsTest, NewTabPage) {
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabled,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  cookie_controls()->Update(web_contents());
}

TEST_F(CookieControlsTest, SomeWebSite) {
  // Visiting a website should enable the UI.
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should be notified.
  EXPECT_CALL(*mock(), OnCookiesCountChanged(1, 0));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://example.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Manually trigger a full update to check that the cookie count changed.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 1, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Blocking cookies should update the blocked cookie count.
  EXPECT_CALL(*mock(), OnCookiesCountChanged(1, 1));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/true);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Manually trigger a full update to check that the cookie count changed.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 1, 1));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Navigating somewhere else should reset the cookie count.
  NavigateAndCommit(GURL("https://somethingelse.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  cookie_controls()->Update(web_contents());
}

TEST_F(CookieControlsTest, PreferenceDisabled) {
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling the feature should disable the UI.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabled,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsTest, AllCookiesBlocked) {
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disable all cookies - an OnStatusCallback should get triggered.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disable cookie blocking for example.com.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsTest, DisableForSite) {
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling cookie blocking for example.com should update the ui.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Visiting some other site, should switch back to kEnabled.
  NavigateAndCommit(GURL("https://somethingelse.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Visiting example.com should set status to kDisabledForSite.
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling example.com again should change status to kEnabled.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsTest, Incognito) {
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Create incognito web_contents and
  // content_settings::CookieControlsController.
  std::unique_ptr<content::WebContents> incognito_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), nullptr);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      incognito_web_contents.get(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          incognito_web_contents.get()));
  auto* tester = content::WebContentsTester::For(incognito_web_contents.get());
  MockOldCookieControlsObserver incognito_mock_;
  content_settings::CookieControlsController incognito_cookie_controls(
      CookieSettingsFactory::GetForProfile(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)),
      CookieSettingsFactory::GetForProfile(profile()),
      HostContentSettingsMapFactory::GetForProfile(profile()));
  incognito_cookie_controls.AddObserver(&incognito_mock_);

  // Navigate incognito web_contents to the same URL.
  tester->NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(incognito_mock_,
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  incognito_cookie_controls.Update(incognito_web_contents.get());
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(&incognito_mock_);

  // Allow cookies in regular mode should also allow in incognito but enforced
  // through regular mode.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  EXPECT_CALL(incognito_mock_,
              OnStatusChanged(
                  CookieControlsStatus::kDisabledForSite,
                  CookieControlsEnforcement::kEnforcedByCookieSetting, 0, 0));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(&incognito_mock_);

  // This should be enforced regardless of the default cookie setting in the
  // default profile.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabled,
                              CookieControlsEnforcement::kNoEnforcement, 0, 0));
  EXPECT_CALL(incognito_mock_,
              OnStatusChanged(
                  CookieControlsStatus::kDisabledForSite,
                  CookieControlsEnforcement::kEnforcedByCookieSetting, 0, 0));
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kIncognitoOnly));
  incognito_cookie_controls.Update(incognito_web_contents.get());
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(&incognito_mock_);
}

TEST_F(CookieControlsTest, CookieBlockingChanged) {
  // Check that the controller correctly keeps track of whether the effective
  // cookie blocking setting for the page has been changed by
  // OnCookieBlockingEnabledForSite().
  cookie_controls()->Update(web_contents());
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_FALSE(cookie_controls()->HasCookieBlockingChangedForSite());

  // Setting to the same effective value should not result in a change.
  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  EXPECT_FALSE(cookie_controls()->HasCookieBlockingChangedForSite());

  // While a different one, should.
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  EXPECT_TRUE(cookie_controls()->HasCookieBlockingChangedForSite());

  // Setting it back should clear it.
  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  EXPECT_FALSE(cookie_controls()->HasCookieBlockingChangedForSite());

  // Navigating to the same page should clear it.
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  EXPECT_TRUE(cookie_controls()->HasCookieBlockingChangedForSite());
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_FALSE(cookie_controls()->HasCookieBlockingChangedForSite());

  // Navigating to a different page should also clear it.
  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  EXPECT_TRUE(cookie_controls()->HasCookieBlockingChangedForSite());
  NavigateAndCommit(GURL("https://thirdparty.com"));
  EXPECT_FALSE(cookie_controls()->HasCookieBlockingChangedForSite());
}

class CookieControlsUserBypassTest : public CookieControlsTest,
                                     public testing::WithParamInterface<bool> {
 public:
  CookieControlsUserBypassTest() {
    std::string expiration = GetParam() ? "90h" : "0h";
    feature_list_.InitWithFeaturesAndParameters(
        {{content_settings::features::kUserBypassUI,
          {{"expiration", expiration}}}},
        {});
  }

 protected:
  void SetUp() override {
    CookieControlsTest::SetUp();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    cookie_controls()->AddObserver(mock());
    testing::Mock::VerifyAndClearExpectations(mock());
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
    auto* entry = entries.front();

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

 private:
  MockCookieControlsObserver mock_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

TEST_P(CookieControlsUserBypassTest, SiteCounts) {
  base::HistogramTester t;

  // Visiting a website should enable the UI.
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing 1p cookies should not increase count.
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://example.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should be notified.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Manually trigger a full update to check that the sites count changed.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Blocking cookies should update the blocked sites count.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 1));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://anotherthirdparty.com")),
      /*blocked_by_policy=*/true);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Manually trigger a full update to check that the sites count changed.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 1));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // A site accessing cookies should be counted only once.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 1));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Another site accessing cookies should update the sites count.
  EXPECT_CALL(*mock(), OnSitesCountChanged(2, 1));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://anothersite.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling third-party cookies records metrics.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                      CookieControlsEnforcement::kNoEnforcement, expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(2, 1));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  t.ExpectUniqueSample(kCookieControlsActivatedSaaHistogram, false, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedRefreshCountHistogram, 0, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedSiteEngagementHistogram, 0, 1);
  t.ExpectUniqueSample(
      kCookieControlsActivatedSiteDataAccessHistogram,
      ThirdPartySiteDataAccessType::kAnyBlockedThirdPartySiteAccesses, 1);

  // Navigating somewhere else should reset the sites count.
  NavigateAndCommit(GURL("https://somethingelse.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
}

TEST_P(CookieControlsUserBypassTest, NewTabPage) {
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(),
              OnBreakageConfidenceLevelChanged(
                  CookieControlsBreakageConfidenceLevel::kUninitialized));
  cookie_controls()->Update(web_contents());
}

TEST_P(CookieControlsUserBypassTest, PreferenceDisabled) {
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling the feature should disable the UI.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(),
              OnBreakageConfidenceLevelChanged(
                  CookieControlsBreakageConfidenceLevel::kUninitialized));
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_P(CookieControlsUserBypassTest, AllCookiesBlocked) {
  base::HistogramTester t;
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disable all cookies - an OnStatusCallback should get triggered.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disable cookie blocking for example.com.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                      CookieControlsEnforcement::kNoEnforcement, expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  t.ExpectUniqueSample(kCookieControlsActivatedSaaHistogram, false, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedRefreshCountHistogram, 0, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedSiteEngagementHistogram, 0, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedSiteDataAccessHistogram,
                       ThirdPartySiteDataAccessType::kNoThirdPartySiteAccesses,
                       1);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_P(CookieControlsUserBypassTest, DisableForSite) {
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling cookie blocking for example.com should update the ui.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                      CookieControlsEnforcement::kNoEnforcement, expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Visiting some other site, should switch back to kEnabled.
  NavigateAndCommit(GURL("https://somethingelse.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Visiting example.com should set status to kDisabledForSite.
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                      CookieControlsEnforcement::kNoEnforcement, expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling example.com again should change status to kEnabled.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_P(CookieControlsUserBypassTest, Incognito) {
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Create incognito web_contents and
  // content_settings::CookieControlsController.
  std::unique_ptr<content::WebContents> incognito_web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), nullptr);
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      incognito_web_contents.get(),
      std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
          incognito_web_contents.get()));
  auto* tester = content::WebContentsTester::For(incognito_web_contents.get());
  MockCookieControlsObserver incognito_mock_;
  content_settings::CookieControlsController incognito_cookie_controls(
      CookieSettingsFactory::GetForProfile(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)),
      CookieSettingsFactory::GetForProfile(profile()),
      HostContentSettingsMapFactory::GetForProfile(profile()));
  incognito_cookie_controls.AddObserver(&incognito_mock_);

  // Navigate incognito web_contents to the same URL.
  tester->NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(incognito_mock_,
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(incognito_mock_, OnSitesCountChanged(0, 0));
  EXPECT_CALL(incognito_mock_,
              OnBreakageConfidenceLevelChanged(
                  CookieControlsBreakageConfidenceLevel::kLow));
  incognito_cookie_controls.Update(incognito_web_contents.get());
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(&incognito_mock_);

  // Allow cookies in regular mode should also allow in incognito but enforced
  // through regular mode.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                      CookieControlsEnforcement::kNoEnforcement, expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  EXPECT_CALL(
      incognito_mock_,
      OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                      CookieControlsEnforcement::kEnforcedByCookieSetting,
                      expiration()));
  EXPECT_CALL(incognito_mock_, OnSitesCountChanged(0, 0));
  EXPECT_CALL(incognito_mock_,
              OnBreakageConfidenceLevelChanged(
                  CookieControlsBreakageConfidenceLevel::kMedium));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(&incognito_mock_);

  // This should be enforced regardless of the default cookie setting in the
  // default profile.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              // Although there is an allow exception with an
                              // expiration, because the default allow never
                              // expires, zero_expiration is correct.
                              zero_expiration()));
  EXPECT_CALL(
      incognito_mock_,
      OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                      CookieControlsEnforcement::kEnforcedByCookieSetting,
                      expiration()));
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kIncognitoOnly));
  incognito_cookie_controls.Update(incognito_web_contents.get());
  testing::Mock::VerifyAndClearExpectations(mock());
  testing::Mock::VerifyAndClearExpectations(&incognito_mock_);
}

TEST_P(CookieControlsUserBypassTest, ThirdPartyCookiesException) {
  // Create third party cookies exception.
  cookie_settings()->SetThirdPartyCookieSetting(
      GURL("https://example.com"), ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateAndCommit(GURL("https://example.com"));
  // Third-party cookie exceptions are handled in the same way as exceptions
  // created for user bypass.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling 3PC for example.com again should change status to kEnabled.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_P(CookieControlsUserBypassTest, FrequentPageReloads) {
  // Update on the initial web contents to ensure the tab observer is setup.
  cookie_controls()->Update(web_contents());
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());

  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  NavigateAndCommit(GURL("https://example.com"));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should be notified.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Reload the page and simulate accessing storage on page load.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // ...and again.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // After the third reload and accessing storage, the confidence level is high.
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kHigh));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  cookie_controls()->OnEntryPointAnimated();
  testing::Mock::VerifyAndClearExpectations(mock());

  // After the entry point was animated (due to high confidence signal), a
  // setting is recorded.
  base::Value stored_value =
      hcsm->GetWebsiteSetting(GURL("https://example.com"), GURL(),
                              ContentSettingsType::COOKIE_CONTROLS_METADATA);
  EXPECT_TRUE(stored_value.is_dict());
  EXPECT_TRUE(stored_value.GetDict().FindBool("entry_point_animated").value());

  // Visiting some other site that have access site data, should reset the
  // confidence level.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  NavigateAndCommit(GURL("https://somethingelse.com"));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://anotherthirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_P(CookieControlsUserBypassTest, FrequestPageReloadsMetrics) {
  base::HistogramTester t;
  cookie_controls()->Update(web_contents());

  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should be notified.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Reload the page and simulate accessing storage on page load.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // ...and again.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // After the third reload and accessing storage, the confidence level is high.
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kHigh));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling third-party cookies records metrics.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                      CookieControlsEnforcement::kNoEnforcement, expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  t.ExpectUniqueSample(kCookieControlsActivatedSaaHistogram, false, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedRefreshCountHistogram, 3, 1);
  t.ExpectUniqueSample(kCookieControlsActivatedSiteEngagementHistogram, 0, 1);
  t.ExpectUniqueSample(
      kCookieControlsActivatedSiteDataAccessHistogram,
      ThirdPartySiteDataAccessType::kAnyAllowedThirdPartySiteAccesses, 1);
  ValidateCookieControlsActivatedUKM(
      /*fed_cm_initiated=*/false,
      /*storage_access_api_requested=*/false,
      /*page_refresh_count=*/3,  // Count was reset to 0 after timeout.
      /*repeated_activation=*/false, blink::mojom::EngagementLevel::NONE,
      ThirdPartySiteDataAccessType::kAnyAllowedThirdPartySiteAccesses);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_P(CookieControlsUserBypassTest, InfrequentPageReloads) {
  base::HistogramTester t;
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should be notified.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Reload the page and simulate accessing storage on page load.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // ...and again.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Wait for 30 seconds.
  FastForwardBy(base::Seconds(30));

  // The third reload happens with a delay and doesn't trigger the confidence
  // level change.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling third-party cookies records metrics.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                      CookieControlsEnforcement::kNoEnforcement, expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
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

TEST_P(CookieControlsUserBypassTest, HighSiteEngagement) {
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
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Without site data access the confidence level cannot be higher than low.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kHigh));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  cookie_controls()->OnEntryPointAnimated();
  testing::Mock::VerifyAndClearExpectations(mock());

  // After the entry point was animated (due to high confidence signal), a
  // setting is recorded.
  base::Value stored_value =
      hcsm->GetWebsiteSetting(GURL("https://highengagement.com"), GURL(),
                              ContentSettingsType::COOKIE_CONTROLS_METADATA);
  EXPECT_TRUE(stored_value.is_dict());
  EXPECT_TRUE(stored_value.GetDict().FindBool("entry_point_animated").value());

  // Visiting some other site resets the confidence level.
  NavigateAndCommit(GURL("https://somethingelse.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Site with medium or low engagement index that has accessed site data has
  // medium confidence level.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://anotherthirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Revisiting high site engagement site doesn't lead to high confidence signal
  // because the entry point was already animated for that site.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  NavigateAndCommit(GURL("https://highengagement.com"));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_P(CookieControlsUserBypassTest, StorageAccessApiHighSiteEngagement) {
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
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Even though the site has high engagement level, the confidence level is
  // medium because SAA was requested in the site context.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling third-party cookies records metrics.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                      CookieControlsEnforcement::kNoEnforcement, expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
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

TEST_P(CookieControlsUserBypassTest, CustomExceptionsNoWildcardMatchingDomain) {
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());

  NavigateAndCommit(GURL("https://cool.things.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Creating an exception changes the status to kDisabledForSite. The exception
  // doesn't contain wildcards in the domain and isn't enforced.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));

  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("cool.things.com"),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_P(CookieControlsUserBypassTest, CustomExceptionsWildcardMatchingDomain) {
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());

  NavigateAndCommit(GURL("https://cool.things.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Creating an exception changes the status to kDisabledForSite. The exception
  // has wildcard in the domain and cannot be reset, it is enforced by cookie
  // setting.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           CookieControlsStatus::kDisabledForSite,
                           CookieControlsEnforcement::kEnforcedByCookieSetting,
                           zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("[*.]cool.things.com"),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_P(CookieControlsUserBypassTest,
       CustomExceptionsWildcardLessSpecificDomain) {
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());

  NavigateAndCommit(GURL("https://cool.things.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Creating an exception changes the status to kDisabledForSite. The exception
  // has wildcard in the domain and cannot be reset, it is enforced by cookie
  // setting.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           CookieControlsStatus::kDisabledForSite,
                           CookieControlsEnforcement::kEnforcedByCookieSetting,
                           zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("[*.]things.com"),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_P(CookieControlsUserBypassTest, CustomExceptionsDotComWildcard) {
  auto* hcsm = HostContentSettingsMapFactory::GetForProfile(profile());

  NavigateAndCommit(GURL("https://cool.things.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kLow));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Creating an exception changes the status to kDisabledForSite. The exception
  // is set at the TLD level and cannot be reset, it is enforced by cookie
  // setting.
  EXPECT_CALL(*mock(), OnStatusChanged(
                           CookieControlsStatus::kDisabledForSite,
                           CookieControlsEnforcement::kEnforcedByCookieSetting,
                           zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  hcsm->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("[*.]com"),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_P(CookieControlsUserBypassTest, FinishedPageReloadWithChangedSettings) {
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
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  ValidateCookieControlsActivatedUKM(
      /*fed_cm_initiated=*/false,
      /*storage_access_api_requested=*/false,
      /*page_refresh_count=*/0, /*repeated_activation=*/false,
      blink::mojom::EngagementLevel::NONE,
      ThirdPartySiteDataAccessType::kNoThirdPartySiteAccesses);

  NavigateAndCommit(GURL("https://example2.com"));
  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  NavigateAndCommit(GURL("https://example2.com"));
}

TEST_P(CookieControlsUserBypassTest, HighConfidenceAfterExpiration) {
  if (!GetParam()) {
    return;
  }

  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/true);
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 1));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enable third-party cookies.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsStatus::kDisabledForSite,
                      CookieControlsEnforcement::kNoEnforcement, expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 1));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
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

  // Visiting the site after exception expires leads to high confidence signal.
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/true);
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 1));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kHigh));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Revisiting the site again doesn't report high confidence signal.
  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://thirdparty.com")),
      /*blocked_by_policy=*/true);
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 1));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_P(CookieControlsUserBypassTest, MediumConfidenceStatefulBounce) {
  if (!GetParam()) {
    return;
  }

  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->IncrementStatefulBounceCount();

  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsStatus::kEnabled,
                              CookieControlsEnforcement::kNoEnforcement,
                              zero_expiration()));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  EXPECT_CALL(*mock(), OnBreakageConfidenceLevelChanged(
                           CookieControlsBreakageConfidenceLevel::kMedium));
  cookie_controls()->Update(web_contents());
}

TEST_P(CookieControlsUserBypassTest, CachedCookieAccessReports) {
  if (!GetParam()) {
    return;
  }

  NavigateAndCommit(GURL("https://example.com"));
  cookie_controls()->Update(web_contents());

  GURL origin1("http://google.com");
  std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
      origin1, "A=B", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(cookie1);

  // Regardless of how many times a cookie access is reported, it should only
  // fire one update.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0)).Times(1);

  for (int i = 0; i < 3; i++) {
    page_specific_content_settings()->OnCookiesAccessed(
        {content::CookieAccessDetails::Type::kChange,
         origin1,
         origin1,
         {*cookie1},
         false});
  }

  // Accessing a cookie for a different origin should however trigger an update.
  GURL origin2("http://another-google.com");
  std::unique_ptr<net::CanonicalCookie> cookie2(net::CanonicalCookie::Create(
      origin2, "A=B", base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */));
  ASSERT_TRUE(cookie2);

  testing::Mock::VerifyAndClearExpectations(mock());
  EXPECT_CALL(*mock(), OnSitesCountChanged(2, 0)).Times(1);
  page_specific_content_settings()->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kChange,
       origin2,
       origin2,
       {*cookie2},
       false});

  // Accessing non cookie storage twice should result in the same site access
  // count being fired twice (no caching).
  testing::Mock::VerifyAndClearExpectations(mock());
  EXPECT_CALL(*mock(), OnSitesCountChanged(3, 0)).Times(2);
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://another-example.com")),
      /*blocked_by_policy=*/false);
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE,
      CreateUnpartitionedStorageKey(GURL("https://another-example.com")),
      /*blocked_by_policy=*/false);

  // Re-navigating to the page should result in the cache being cleared, and
  // cookie access being re-reported.
  testing::Mock::VerifyAndClearExpectations(mock());
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0)).Times(1);

  NavigateAndCommit(GURL("https://example.com"));
  page_specific_content_settings()->OnCookiesAccessed(
      {content::CookieAccessDetails::Type::kChange,
       origin1,
       origin1,
       {*cookie1},
       false});
}

INSTANTIATE_TEST_SUITE_P(All, CookieControlsUserBypassTest, testing::Bool());
