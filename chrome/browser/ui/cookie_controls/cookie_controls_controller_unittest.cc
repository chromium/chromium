// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/cookie_controls_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockCookieControlsView : public CookieControlsView {
 public:
  MOCK_METHOD2(OnStatusChanged, void(CookieControlsController::Status, int));
  MOCK_METHOD1(OnBlockedCookiesCountChanged, void(int));
};

}  // namespace

// More readable output for test expectation.
std::ostream& operator<<(std::ostream& os,
                         const CookieControlsController::Status& status) {
  switch (status) {
    case CookieControlsController::Status::kDisabled:
      return os << "kDisabled";
    case CookieControlsController::Status::kEnabled:
      return os << "kEnabled";
    case CookieControlsController::Status::kDisabledForSite:
      return os << "kDisabledForSite";
    case CookieControlsController::Status::kUninitialized:
      return os << "kUninitialized";
  }
}

class CookieControlsTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    feature_list.InitAndEnableFeature(
        content_settings::kImprovedCookieControls);
    ChromeRenderViewHostTestHarness::SetUp();
    TabSpecificContentSettings::CreateForWebContents(web_contents());
    profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(content_settings::CookieControlsMode::kOn));
    NavigateAndCommit(GURL("chrome://new-tab"));

    cookie_controls_ =
        std::make_unique<CookieControlsController>(web_contents());
    cookie_controls_->AddObserver(mock());
    testing::Mock::VerifyAndClearExpectations(mock());
  }

  void TearDown() override {
    cookie_controls_->RemoveObserver(mock());
    cookie_controls_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  CookieControlsController* cookie_controls() { return cookie_controls_.get(); }
  MockCookieControlsView* mock() { return &mock_; }

  TabSpecificContentSettings* tab_specific_content_settings() {
    return TabSpecificContentSettings::FromWebContents(web_contents());
  }

 private:
  base::test::ScopedFeatureList feature_list;
  MockCookieControlsView mock_;
  std::unique_ptr<CookieControlsController> cookie_controls_;
};

TEST_F(CookieControlsTest, NewTabPage) {
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsController::Status::kDisabled, 0));
  cookie_controls()->Update(web_contents());
}

TEST_F(CookieControlsTest, SomeWebSite) {
  // Visiting a website should enable the UI.
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsController::Status::kEnabled, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Accessing cookies should not change anything.
  EXPECT_CALL(*mock(), OnBlockedCookiesCountChanged(0));
  tab_specific_content_settings()->OnWebDatabaseAccessed(
      GURL("https://example.com"), /*blocked=*/false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Blocking cookies should update the blocked cookie count.
  EXPECT_CALL(*mock(), OnBlockedCookiesCountChanged(1));
  tab_specific_content_settings()->OnWebDatabaseAccessed(
      GURL("https://thirdparty.com"), /*blocked=*/true);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Navigating somewhere else should reset the cookie count.
  NavigateAndCommit(GURL("https://somethingelse.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsController::Status::kEnabled, 0));
  cookie_controls()->Update(web_contents());
}

TEST_F(CookieControlsTest, PreferenceDisabled) {
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsController::Status::kEnabled, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling the feature should disable the UI.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsController::Status::kDisabled, 0));
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));
  testing::Mock::VerifyAndClearExpectations(mock());
}

TEST_F(CookieControlsTest, DisableForSite) {
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsController::Status::kEnabled, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Disabling cookie blocking for example.com should update the ui.
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsController::Status::kDisabledForSite, 0));
  cookie_controls()->OnCookieBlockingEnabledForSite(false);
  testing::Mock::VerifyAndClearExpectations(mock());

  // Visiting some other site, should switch back to kEnabled.
  NavigateAndCommit(GURL("https://somethingelse.com"));
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsController::Status::kEnabled, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Visiting example.com should set status to kDisabledForSite.
  NavigateAndCommit(GURL("https://example.com"));
  EXPECT_CALL(
      *mock(),
      OnStatusChanged(CookieControlsController::Status::kDisabledForSite, 0));
  cookie_controls()->Update(web_contents());
  testing::Mock::VerifyAndClearExpectations(mock());

  // Enabling example.com again should change status to kEnabled.
  EXPECT_CALL(*mock(),
              OnStatusChanged(CookieControlsController::Status::kEnabled, 0));
  cookie_controls()->OnCookieBlockingEnabledForSite(true);
  testing::Mock::VerifyAndClearExpectations(mock());
}
