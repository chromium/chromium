// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/passwords/google_password_manager_navigation_throttle.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "url/gurl.h"
#include "url/url_canon_stdstring.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#endif

namespace {

using content::TestNavigationObserver;

GURL GetExampleURL() {
  return GURL("https://example.com");
}

GURL GetGooglePasswordManagerURL() {
  return GURL(chrome::kGooglePasswordManagerURL);
}

GURL GetPasswordSettingsURL() {
  return chrome::GetSettingsUrl(chrome::kPasswordManagerSubPage);
}

// Starts to navigate to the |url| and then returns the GURL that ultimately was
// navigated to.
GURL NavigateToURL(Browser* browser,
                   const GURL& url,
                   ui::PageTransition transition) {
  NavigateParams params(browser, url, transition);
  Navigate(&params);
  TestNavigationObserver observer(params.navigated_or_inserted_contents);
  observer.Wait();
  return observer.last_navigation_url();
}

}  // namespace

class GooglePasswordManagerNavigationThrottleTest : public SyncTest {
 public:
  GooglePasswordManagerNavigationThrottleTest()
      : SyncTest(TestType::SINGLE_CLIENT),
        interceptor_(
            std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
                [](content::URLLoaderInterceptor::RequestParams* params) {
                  params->client->OnComplete(
                      network::URLLoaderCompletionStatus(net::ERR_FAILED));
                  return true;
                }))) {}

 protected:
  void TearDownOnMainThread() override {
    interceptor_.reset();
    SyncTest::TearDownOnMainThread();
  }

 private:
  // Instantiate a content::URLLoaderInterceptor that will fail all requests
  // with net::ERR_FAILED. This is done, because we are interested in being
  // redirected when a navigation fails.
  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;
};

class GooglePasswordManagerNavigationThrottleTestWithPasswordManager
    : public GooglePasswordManagerNavigationThrottleTest {
 public:
  GooglePasswordManagerNavigationThrottleTestWithPasswordManager() {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kGooglePasswordManager);
  }

  std::unique_ptr<ProfileSyncServiceHarness> EnableSync(Profile* profile) {
    ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(profile)
        ->OverrideNetworkForTest(
            fake_server::CreateFakeServerHttpPostProviderFactory(
                GetFakeServer()->AsWeakPtr()));

    std::string username;
#if defined(OS_CHROMEOS)
    // In browser tests, the profile may already be authenticated with stub
    // account |user_manager::kStubUserEmail|.
    CoreAccountInfo info =
        IdentityManagerFactory::GetForProfile(profile)->GetPrimaryAccountInfo();
    username = info.email;
#endif
    if (username.empty())
      username = "user@gmail.com";

    std::unique_ptr<ProfileSyncServiceHarness> harness =
        ProfileSyncServiceHarness::Create(
            profile, username, "password",
            ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN);

    EXPECT_TRUE(harness->SetupSync());
    return harness;
  }
};

// No navigation should be redirected in case the Google Password Manager and
// Sync are not enabled.
IN_PROC_BROWSER_TEST_F(GooglePasswordManagerNavigationThrottleTest,
                       ExampleWithoutGPMAndSync) {
  EXPECT_EQ(GetExampleURL(),
            NavigateToURL(browser(), GetExampleURL(),
                          ui::PageTransition::PAGE_TRANSITION_LINK));
}

IN_PROC_BROWSER_TEST_F(GooglePasswordManagerNavigationThrottleTest,
                       PasswordsWithoutGPMAndSync) {
  EXPECT_EQ(GetGooglePasswordManagerURL(),
            NavigateToURL(browser(), GetGooglePasswordManagerURL(),
                          ui::PageTransition::PAGE_TRANSITION_LINK));
}

// Accessing a web resource from within this browser test will fail (see
// |interceptor_| above), thus we expect to be redirected to the Passwords
// Settings Subpage when trying to access the Google Password Manager when the
// user's profile should be considered and the user clicked a link to get to the
// Google Password Manager page.
IN_PROC_BROWSER_TEST_F(
    GooglePasswordManagerNavigationThrottleTestWithPasswordManager,
    ExampleWithGPMAndSync) {
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSync(browser()->profile());
  EXPECT_EQ(GetExampleURL(),
            NavigateToURL(browser(), GetExampleURL(),
                          ui::PageTransition::PAGE_TRANSITION_LINK));
}

IN_PROC_BROWSER_TEST_F(
    GooglePasswordManagerNavigationThrottleTestWithPasswordManager,
    PasswordsWithGPMAndSyncUserTyped) {
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSync(browser()->profile());
  EXPECT_EQ(GetGooglePasswordManagerURL(),
            NavigateToURL(browser(), GetGooglePasswordManagerURL(),
                          ui::PageTransition::PAGE_TRANSITION_TYPED));
}

IN_PROC_BROWSER_TEST_F(
    GooglePasswordManagerNavigationThrottleTestWithPasswordManager,
    PasswordsWithGPMAndSyncUserClickedLink) {
  base::HistogramTester tester;
  std::unique_ptr<ProfileSyncServiceHarness> harness =
      EnableSync(browser()->profile());

#if defined(OS_CHROMEOS)
  // Install the Settings App.
  web_app::WebAppProvider::Get(browser()->profile())
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();
#endif

  TestNavigationObserver settings_observer(GetPasswordSettingsURL());
  settings_observer.StartWatchingNewWebContents();
  NavigateToGooglePasswordManager(
      browser()->profile(),
      password_manager::ManagePasswordsReferrer::kProfileChooser);
  settings_observer.Wait();

  EXPECT_TRUE(settings_observer.last_navigation_succeeded());
  EXPECT_EQ(GetPasswordSettingsURL(), settings_observer.last_navigation_url());

  tester.ExpectUniqueSample(
      "PasswordManager.GooglePasswordManager.NavigationResult",
      GooglePasswordManagerNavigationThrottle::NavigationResult::kFailure, 1);
  tester.ExpectTotalCount("PasswordManager.GooglePasswordManager.TimeToFailure",
                          1);
  tester.ExpectTotalCount("PasswordManager.GooglePasswordManager.TimeToSuccess",
                          0);
}
