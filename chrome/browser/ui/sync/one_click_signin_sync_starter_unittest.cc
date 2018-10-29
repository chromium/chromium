// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sync/one_click_signin_sync_starter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestingGaiaId[] = "gaia_id";
const char kTestingRefreshToken[] = "refresh_token";
const char kTestingUsername[] = "fake_username";

}  // namespace

class OneClickSigninSyncStarterTest : public ChromeRenderViewHostTestHarness {
 public:
  OneClickSigninSyncStarterTest()
      : sync_starter_(nullptr), failed_count_(0), succeeded_count_(0) {}

  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Disable sync to simplify the creation of a OneClickSigninSyncStarter.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableSync);

    SigninManagerBase* signin_manager =
        SigninManagerFactory::GetForProfile(profile());

    signin_manager->Initialize(nullptr);
    signin_manager->SetAuthenticatedAccountInfo(kTestingGaiaId,
                                                kTestingUsername);
  }

  void Callback(OneClickSigninSyncStarter::SyncSetupResult result) {
    if (result == OneClickSigninSyncStarter::SYNC_SETUP_SUCCESS)
      ++succeeded_count_;
    else
      ++failed_count_;
  }

  // ChromeRenderViewHostTestHarness:
  content::BrowserContext* CreateBrowserContext() override {
    // Create the sign in manager required by OneClickSigninSyncStarter.
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        SigninManagerFactory::GetInstance(),
        base::BindRepeating(
            &OneClickSigninSyncStarterTest::BuildSigninManager));
    return builder.Build().release();
  }

 protected:
  void CreateSyncStarter(OneClickSigninSyncStarter::Callback callback) {
    sync_starter_ = new OneClickSigninSyncStarter(
        profile(), nullptr, kTestingGaiaId, kTestingUsername, std::string(),
        kTestingRefreshToken, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
        signin_metrics::Reason::REASON_UNKNOWN_REASON,
        OneClickSigninSyncStarter::CURRENT_PROFILE,
        OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS,
        OneClickSigninSyncStarter::NO_CONFIRMATION, callback);
  }

  // Deletes itself when SigninFailed() or SigninSuccess() is called.
  OneClickSigninSyncStarter* sync_starter_;

  // Number of times that the callback is called with SYNC_SETUP_FAILURE.
  int failed_count_;

  // Number of times that the callback is called with SYNC_SETUP_SUCCESS.
  int succeeded_count_;

 private:
  static std::unique_ptr<KeyedService> BuildSigninManager(
      content::BrowserContext* context) {
    Profile* profile = static_cast<Profile*>(context);
    return std::make_unique<FakeSigninManager>(
        ChromeSigninClientFactory::GetForProfile(profile),
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
        AccountTrackerServiceFactory::GetForProfile(profile),
        GaiaCookieManagerServiceFactory::GetForProfile(profile));
  }

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninSyncStarterTest);
};

// Verifies that the callback is invoked when sync setup fails.
TEST_F(OneClickSigninSyncStarterTest, CallbackSigninFailed) {
  CreateSyncStarter(base::Bind(&OneClickSigninSyncStarterTest::Callback,
                               base::Unretained(this)));
  sync_starter_->SigninFailed(GoogleServiceAuthError(
      GoogleServiceAuthError::REQUEST_CANCELED));
  EXPECT_EQ(1, failed_count_);
  EXPECT_EQ(0, succeeded_count_);
}

// Verifies that there is no crash when the callback is NULL.
TEST_F(OneClickSigninSyncStarterTest, CallbackNull) {
  CreateSyncStarter(OneClickSigninSyncStarter::Callback());
  sync_starter_->SigninFailed(GoogleServiceAuthError(
      GoogleServiceAuthError::REQUEST_CANCELED));
  EXPECT_EQ(0, failed_count_);
  EXPECT_EQ(0, succeeded_count_);
}
