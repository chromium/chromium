// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adapted version of the tests.

#include "chrome/browser/ui/webui/settings/people_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_web_ui.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings {

using signin::ConsentLevel;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

class TestingPeopleHandler : public PeopleHandler {
 public:
  TestingPeopleHandler(content::WebUI* web_ui, Profile* profile)
      : PeopleHandler(profile) {
    set_web_ui(web_ui);
  }

  TestingPeopleHandler(const TestingPeopleHandler&) = delete;
  TestingPeopleHandler& operator=(const TestingPeopleHandler&) = delete;

  using PeopleHandler::is_configuring_sync;
};

class PeopleHandlerSignoutTest : public SigninBrowserTestBase {
 public:
  PeopleHandlerSignoutTest() = default;
  ~PeopleHandlerSignoutTest() override = default;

  PeopleHandler* handler() { return handler_.get(); }

  void CreatePeopleHandler() {
    handler_ = std::make_unique<TestingPeopleHandler>(&web_ui_, GetProfile());
  }

  void SimulateSignout(const base::ListValue& args) {
    handler()->HandleSignout(args);
  }

  content::WebUI* web_ui() { return handler()->web_ui(); }

  content::WebContents* web_contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

 protected:
  void SetUpOnMainThread() override {
    SigninBrowserTestBase::SetUpOnMainThread();

    // Navigate to NTP to match the original test setup.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                             GURL(chrome::kChromeUINewTabURL)));

    web_ui_.set_web_contents(web_contents());
  }

  void TearDownOnMainThread() override {
    if (handler_) {
      handler_->set_web_ui(nullptr);
      handler_->DisallowJavascript();
    }
    SigninBrowserTestBase::TearDownOnMainThread();
  }

  SigninClient* GetSigninClient(Profile* profile) {
    return ChromeSigninClientFactory::GetForProfile(profile);
  }

 private:
  content::TestWebUI web_ui_;
  std::unique_ptr<TestingPeopleHandler> handler_;
};

#if DCHECK_IS_ON()
IN_PROC_BROWSER_TEST_F(PeopleHandlerSignoutTest, SignoutNotAllowedSyncOff) {
  auto account_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "a@gmail.com", ConsentLevel::kSignin);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  GetSigninClient(GetProfile())
      ->set_is_clear_primary_account_allowed_for_testing(
          SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);

  CreatePeopleHandler();

  base::ListValue args;
  args.Append(/*value=*/false);
  EXPECT_DEATH(SimulateSignout(args), ".*");
}
#endif  // DCHECK_IS_ON()

IN_PROC_BROWSER_TEST_F(PeopleHandlerSignoutTest, SignoutNotAllowedSyncOn) {
  auto account_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "a@gmail.com", ConsentLevel::kSync);
  auto account_2 = identity_test_env()->MakeAccountAvailable("b@gmail.com");
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_EQ(2U, identity_manager()->GetAccountsWithRefreshTokens().size());
  GetSigninClient(GetProfile())
      ->set_is_clear_primary_account_allowed_for_testing(
          SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);

  CreatePeopleHandler();

  base::ListValue args;
  args.Append(/*value=*/false);
  SimulateSignout(args);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_EQ(2U, identity_manager()->GetAccountsWithRefreshTokens().size());

  // Signout not triggered on dice platforms.
  EXPECT_EQ(web_contents()->GetVisibleURL().spec(), chrome::kChromeUINewTabURL);
  EXPECT_NE(web_contents()->GetVisibleURL(),
            GaiaUrls::GetInstance()->service_logout_url());
}

IN_PROC_BROWSER_TEST_F(PeopleHandlerSignoutTest, SignoutWithSyncOn) {
  auto account_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "a@gmail.com", ConsentLevel::kSync);
  auto account_2 = identity_test_env()->MakeAccountAvailable("b@gmail.com");
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_EQ(2U, identity_manager()->GetAccountsWithRefreshTokens().size());

  CreatePeopleHandler();

  EXPECT_NE(web_ui(), nullptr);
  EXPECT_NE(nullptr, web_ui()->GetWebContents());

  EXPECT_TRUE(GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
      web_ui()->GetWebContents()));

  base::ListValue args;
  args.Append(/*value=*/false);
  SimulateSignout(args);

  EXPECT_EQ(web_contents()->GetVisibleURL(),
            GaiaUrls::GetInstance()->LogOutURLWithContinueURL(GURL()));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
}

IN_PROC_BROWSER_TEST_F(PeopleHandlerSignoutTest, Signout) {
  auto account_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "a@gmail.com", ConsentLevel::kSignin);
  auto account_2 = identity_test_env()->MakeAccountAvailable("b@gmail.com");
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_EQ(2U, identity_manager()->GetAccountsWithRefreshTokens().size());

  CreatePeopleHandler();
  EXPECT_FALSE(SigninViewController::From(browser())->ShowsModalDialog());

  // Set up observer to wait for the signout confirmation dialog.
  auto url = GURL(chrome::kChromeUISignoutConfirmationURL);
  content::TestNavigationObserver observer(url);
  observer.StartWatchingNewWebContents();

  base::ListValue args;
  args.Append(/*value=*/false);
  SimulateSignout(args);

  // Wait for the dialog to load.
  observer.Wait();

  // The signout confirmation dialog is shown.
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_TRUE(SigninViewController::From(browser())->ShowsModalDialog());
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace settings
