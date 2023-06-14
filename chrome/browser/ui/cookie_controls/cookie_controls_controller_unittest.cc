// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/ui/cookie_controls_controller.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
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
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using StorageType =
    content_settings::mojom::ContentSettingsManager::StorageType;

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
              (CookieControlsStatus,
               CookieControlsEnforcement,
               absl::optional<base::Time>));
  MOCK_METHOD(void, OnSitesCountChanged, (int, int));
  MOCK_METHOD(void,
              OnBreakageConfidenceLevelChanged,
              (CookieControlsBreakageConfidenceLevel));
};

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
            cookie_settings_, nullptr);
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
      StorageType::DATABASE, GURL("https://example.com"),
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
      StorageType::DATABASE, GURL("https://thirdparty.com"),
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
      CookieSettingsFactory::GetForProfile(profile()));
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
}

class CookieControlsUserBypassTest : public CookieControlsTest {
 public:
  CookieControlsUserBypassTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{content_settings::features::kUserBypassUI, {{"expiration", "0d"}}}},
        {});
  }

 protected:
  void SetUp() override {
    CookieControlsTest::SetUp();

    cookie_controls()->AddObserver(mock());
    testing::Mock::VerifyAndClearExpectations(mock());
  }

  MockCookieControlsObserver* mock() { return &mock_; }

 private:
  MockCookieControlsObserver mock_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(CookieControlsUserBypassTest, SiteCounts) {
  absl::optional<base::Time> expiration = absl::nullopt;

  // Visiting a website should enable the UI.
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsStatus::kEnabled,
                      CookieControlsEnforcement::kNoEnforcement, expiration));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should be notified.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE, GURL("https://example.com"),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Manually trigger a full update to check that the sites count changed.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsStatus::kEnabled,
                      CookieControlsEnforcement::kNoEnforcement, expiration));
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Blocking cookies should update the blocked sites count.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 1));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE, GURL("https://thirdparty.com"),
      /*blocked_by_policy=*/true);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Manually trigger a full update to check that the sites count changed.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsStatus::kEnabled,
                      CookieControlsEnforcement::kNoEnforcement, expiration));
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 1));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // A site accessing cookies should be counted only once.
  EXPECT_CALL(*mock(), OnSitesCountChanged(1, 1));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE, GURL("https://example.com"),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Another site accessing cookies should update the sites count.
  EXPECT_CALL(*mock(), OnSitesCountChanged(2, 1));
  page_specific_content_settings()->OnStorageAccessed(
      StorageType::DATABASE, GURL("https://anothersite.com"),
      /*blocked_by_policy=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Navigating somewhere else should reset the sites count.
  NavigateAndCommit(GURL("https://somethingelse.com"));
  EXPECT_CALL(*mock(), OnSitesCountChanged(0, 0));
  cookie_controls()->Update(web_contents());
}
