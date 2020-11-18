// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/account_consistency_service.h"

#import <WebKit/WebKit.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/ios/ios_util.h"
#include "base/test/bind_test_util.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ios/web/public/navigation/web_state_policy_decider.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "net/cookies/cookie_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// URL of the Google domain where the CHROME_CONNECTED cookie is set/removed.
NSURL* const kGoogleUrl = [NSURL URLWithString:@"https://google.com/"];
// URL of the Youtube domain where the CHROME_CONNECTED cookie is set/removed.
NSURL* const kYoutubeUrl = [NSURL URLWithString:@"https://youtube.com/"];
// URL of a country Google domain where the CHROME_CONNECTED cookie is
// set/removed.
NSURL* const kCountryGoogleUrl = [NSURL URLWithString:@"https://google.de/"];

// Google domain.
const char* kGoogleDomain = "google.com";
// Youtube domain.
const char* kYoutubeDomain = "youtube.com";
// Google domain where the CHROME_CONNECTED cookie is set/removed.
const char* kCountryGoogleDomain = "google.de";

// Name of the histogram to record whether the GAIA cookie is present.
const char* kGAIACookiePresentHistogram =
    "Signin.IOSGaiaCookiePresentOnNavigation";

// Returns a cookie domain that applies for all origins on |host_domain|.
std::string GetCookieDomain(const std::string& host_domain) {
  DCHECK(net::cookie_util::DomainIsHostOnly(host_domain));
  std::string cookie_domain = "." + host_domain;
  DCHECK(!net::cookie_util::DomainIsHostOnly(cookie_domain));
  return cookie_domain;
}

// Returns true if |cookies| contains a cookie with |name| and |domain|.
//
// Note: If |domain| is the empty string, then it returns true if any cookie
// with |name| is found.
bool ContainsCookie(const std::vector<net::CanonicalCookie>& cookies,
                    const std::string& name,
                    const std::string& domain) {
  for (const auto& cookie : cookies) {
    if (cookie.Name() == name) {
      if (domain.empty() || cookie.Domain() == domain)
        return true;
    }
  }
  return false;
}

// AccountConsistencyService specialization that fakes the creation of the
// WKWebView in order to mock it. This allows tests to intercept the calls to
// the Web view and control they are correct.
class FakeAccountConsistencyService : public AccountConsistencyService {
 public:
  FakeAccountConsistencyService(
      web::BrowserState* browser_state,
      PrefService* prefs,
      AccountReconcilor* account_reconcilor,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      signin::IdentityManager* identity_manager)
      : AccountConsistencyService(browser_state,
                                  prefs,
                                  account_reconcilor,
                                  cookie_settings,
                                  identity_manager) {}

};

// Mock AccountReconcilor to catch call to OnReceivedManageAccountsResponse.
class MockAccountReconcilor : public AccountReconcilor {
 public:
  MockAccountReconcilor(SigninClient* client)
      : AccountReconcilor(
            nullptr,
            client,
            std::make_unique<signin::AccountReconcilorDelegate>()) {}
  MOCK_METHOD1(OnReceivedManageAccountsResponse, void(signin::GAIAServiceType));
};

// TestWebState that allows control over its policy decider.
class TestWebState : public web::TestWebState {
 public:
  TestWebState() : web::TestWebState(), decider_(nullptr) {}
  void AddPolicyDecider(web::WebStatePolicyDecider* decider) override {
    EXPECT_FALSE(decider_);
    decider_ = decider;
  }
  void RemovePolicyDecider(web::WebStatePolicyDecider* decider) override {
    EXPECT_EQ(decider_, decider);
    decider_ = nullptr;
  }
  bool ShouldAllowResponse(NSURLResponse* response, bool for_main_frame) {
    if (!decider_)
      return true;

    __block web::WebStatePolicyDecider::PolicyDecision policyDecision =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback =
        base::Bind(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policyDecision = decision;
        });
    decider_->ShouldAllowResponse(response, for_main_frame,
                                  std::move(callback));
    return policyDecision.ShouldAllowNavigation();
  }
  void WebStateDestroyed() {
    if (!decider_)
      return;
    decider_->WebStateDestroyed();
  }

 private:
  web::WebStatePolicyDecider* decider_;
};

}  // namespace

class AccountConsistencyServiceTest : public PlatformTest {
 public:
  AccountConsistencyServiceTest()
      : task_environment_(web::WebTaskEnvironment::Options::DEFAULT,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void OnRemoveChromeConnectedCookieFinished() {
    EXPECT_FALSE(remove_cookie_callback_called_);
    remove_cookie_callback_called_ = true;
  }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    AccountConsistencyService::RegisterPrefs(prefs_.registry());
    content_settings::CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());

    signin_client_.reset(
        new TestSigninClient(&prefs_, &test_url_loader_factory_));
    identity_test_env_.reset(new signin::IdentityTestEnvironment(
        /*test_url_loader_factory=*/nullptr, &prefs_,
        signin::AccountConsistencyMethod::kDisabled, signin_client_.get()));
    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */);
    cookie_settings_ = new content_settings::CookieSettings(settings_map_.get(),
                                                            &prefs_, false, "");
    account_reconcilor_ =
        std::make_unique<MockAccountReconcilor>(signin_client_.get());
    ResetAccountConsistencyService();
  }

  void TearDown() override {
    account_consistency_service_->Shutdown();
    settings_map_->ShutdownOnUIThread();
    identity_test_env_.reset();
    PlatformTest::TearDown();
  }

  void ResetAccountConsistencyService() {
    if (account_consistency_service_) {
      account_consistency_service_->Shutdown();
    }
    account_consistency_service_.reset(new FakeAccountConsistencyService(
        &browser_state_, &prefs_, account_reconcilor_.get(), cookie_settings_,
        identity_test_env_->identity_manager()));
  }

  void WaitUntilAllCookieRequestsAreApplied() {
    // Spinning the runloop is needed to ensure that the cookie manager requests
    // are executed.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(0, account_consistency_service_
                     ->active_cookie_manager_requests_for_testing_);
  }

  void SignIn() {
    signin::MakePrimaryAccountAvailable(identity_test_env_->identity_manager(),
                                        "user@gmail.com");
    WaitUntilAllCookieRequestsAreApplied();
  }

  void SignOut() {
    signin::ClearPrimaryAccount(identity_test_env_->identity_manager(),
                                signin::ClearPrimaryAccountPolicy::DEFAULT);
    WaitUntilAllCookieRequestsAreApplied();
  }

  std::vector<net::CanonicalCookie> GetCookiesInCookieJar() {
    std::vector<net::CanonicalCookie> cookies_out;
    base::RunLoop run_loop;
    network::mojom::CookieManager* cookie_manager =
        browser_state_.GetCookieManager();
    cookie_manager->GetAllCookies(base::BindOnce(base::BindLambdaForTesting(
        [&run_loop,
         &cookies_out](const std::vector<net::CanonicalCookie>& cookies) {
          cookies_out = cookies;
          run_loop.Quit();
        })));
    run_loop.Run();

    return cookies_out;
  }

  // Returns time the CHROME_CONNECTED cookie was last updated for |domain|.
  base::Time GetCookieLastUpdateTime(const std::string& domain) {
    return account_consistency_service_->last_cookie_update_map_[domain];
  }

  // Returns time the Gaia cookie was last updated for Google domains.
  base::Time GetGaiaLastUpdateTime() {
    return account_consistency_service_->last_gaia_cookie_verification_time_;
  }

  void CheckDomainHasChromeConnectedCookie(const std::string& domain) {
    EXPECT_TRUE(
        ContainsCookie(GetCookiesInCookieJar(),
                       AccountConsistencyService::kChromeConnectedCookieName,
                       GetCookieDomain(domain)));
    EXPECT_GE(
        account_consistency_service_->last_cookie_update_map_.count(domain),
        1u);
  }

  void CheckNoChromeConnectedCookieForDomain(const std::string& domain) {
    EXPECT_FALSE(
        ContainsCookie(GetCookiesInCookieJar(),
                       AccountConsistencyService::kChromeConnectedCookieName,
                       GetCookieDomain(domain)));
    EXPECT_EQ(0U, account_consistency_service_->last_cookie_update_map_.count(
                      domain));
  }

  void CheckNoChromeConnectedCookies() {
    EXPECT_FALSE(
        ContainsCookie(GetCookiesInCookieJar(),
                       AccountConsistencyService::kChromeConnectedCookieName,
                       /*domain=*/std::string()));
  }

  // Simulate navigating to a URL with the given page load completion status.
  void SimulateNavigateToUrl(web::PageLoadCompletionStatus status,
                             const GURL& url) {
    web_state_.SetCurrentURL(url);
    web_state_.OnPageLoaded(status);
  }

  // Simulate the action of the action GaiaCookieManagerService to cleanup
  // the cookies once the sign-out is done.
  void RemoveAllChromeConnectedCookies() {
    base::RunLoop run_loop;
    account_consistency_service_->RemoveAllChromeConnectedCookies(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Simulates setting the CHROME_CONNECTED cookie for the Google domain at the
  // designated time interval. Returns the time at which the cookie was updated.
  void SimulateSetChromeConnectedCookieForGoogleDomain() {
    account_consistency_service_->SetChromeConnectedCookieWithUrls(
        {GURL("https://google.com")});
    WaitUntilAllCookieRequestsAreApplied();
  }

  // Simulates updating the Gaia cookie on the Google domain at the designated
  // time interval. Returns the time at which the cookie was updated.
  void SimulateUpdateGaiaCookie() {
    account_consistency_service_->SetGaiaCookiesIfDeleted();
  }

  void CheckGoogleDomainHasGaiaCookie() {
    EXPECT_TRUE(ContainsCookie(GetCookiesInCookieJar(),
                               AccountConsistencyService::kGaiaCookieName,
                               ".google.com"));
  }

  // Creates test threads, necessary for ActiveStateManager that needs a UI
  // thread.
  web::WebTaskEnvironment task_environment_;
  web::TestBrowserState browser_state_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestWebState web_state_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  // AccountConsistencyService being tested. Actually a
  // FakeAccountConsistencyService to be able to use a mock web view.
  std::unique_ptr<AccountConsistencyService> account_consistency_service_;
  std::unique_ptr<TestSigninClient> signin_client_;
  std::unique_ptr<MockAccountReconcilor> account_reconcilor_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  bool remove_cookie_callback_called_;
};

// Tests that main domains are added to the internal map when cookies are set in
// reaction to signin.
TEST_F(AccountConsistencyServiceTest, SigninAddCookieOnMainDomains) {
  SignIn();
  CheckDomainHasChromeConnectedCookie(kGoogleDomain);
  CheckDomainHasChromeConnectedCookie(kYoutubeDomain);

  const base::DictionaryValue* dict =
      prefs_.GetDictionary(AccountConsistencyService::kDomainsWithCookiePref);
  EXPECT_EQ(2u, dict->size());
  EXPECT_TRUE(dict->GetBooleanWithoutPathExpansion("google.com", nullptr));
  EXPECT_TRUE(dict->GetBooleanWithoutPathExpansion("youtube.com", nullptr));
}

// Tests that cookies that are added during SignIn and subsequent navigations
// are correctly removed during the SignOut.
TEST_F(AccountConsistencyServiceTest, SignInSignOut) {
  SignIn();
  CheckDomainHasChromeConnectedCookie(kGoogleDomain);
  CheckDomainHasChromeConnectedCookie(kYoutubeDomain);
  CheckNoChromeConnectedCookieForDomain(kCountryGoogleDomain);

  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];
  NSDictionary* headers = [NSDictionary dictionary];
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:kCountryGoogleUrl
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:headers];
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
  EXPECT_TRUE(
      web_state_.ShouldAllowResponse(response, /* for_main_frame = */ true));
  web_state_.WebStateDestroyed();

  // Check that cookies was also added for |kCountryGoogleDomain|.
  CheckDomainHasChromeConnectedCookie(kGoogleDomain);
  CheckDomainHasChromeConnectedCookie(kYoutubeDomain);
  CheckDomainHasChromeConnectedCookie(kCountryGoogleDomain);

  SignOut();
  CheckNoChromeConnectedCookies();
}

// Tests that signing out with no domains known, still call the callback.
TEST_F(AccountConsistencyServiceTest, SignOutWithoutDomains) {
  CheckNoChromeConnectedCookies();

  SignOut();
  CheckNoChromeConnectedCookies();
}

// Tests that the X-Chrome-Manage-Accounts header is ignored unless it comes
// from Gaia signon realm.
TEST_F(AccountConsistencyServiceTest, ChromeManageAccountsNotOnGaia) {
  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];

  NSDictionary* headers =
      [NSDictionary dictionaryWithObject:@"action=DEFAULT"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://google.com"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
  EXPECT_TRUE(
      web_state_.ShouldAllowResponse(response, /* for_main_frame = */ true));
  web_state_.WebStateDestroyed();

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that navigation to Gaia signon realm with no X-Chrome-Manage-Accounts
// header in the response are simply untouched.
TEST_F(AccountConsistencyServiceTest, ChromeManageAccountsNoHeader) {
  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];

  NSDictionary* headers = [NSDictionary dictionary];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
  EXPECT_TRUE(
      web_state_.ShouldAllowResponse(response, /* for_main_frame = */ true));
  web_state_.WebStateDestroyed();

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the ManageAccountsDelegate is notified when a navigation on Gaia
// signon realm returns with a X-Chrome-Manage-Accounts header with action
// DEFAULT.
TEST_F(AccountConsistencyServiceTest, ChromeManageAccountsDefault) {
  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];
  // Default action is |onManageAccounts|.
  [[delegate expect] onManageAccounts];

  NSDictionary* headers =
      [NSDictionary dictionaryWithObject:@"action=DEFAULT"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
  EXPECT_CALL(*account_reconcilor_, OnReceivedManageAccountsResponse(
                                        signin::GAIA_SERVICE_TYPE_DEFAULT))
      .Times(1);
  EXPECT_FALSE(
      web_state_.ShouldAllowResponse(response, /* for_main_frame = */ true));
  web_state_.WebStateDestroyed();

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the ManageAccountsDelegate is notified when a navigation on Gaia
// signon realm returns with a X-Chrome-Manage-Accounts header with show
// consistency promo and ADDSESSION action.
TEST_F(AccountConsistencyServiceTest,
       ChromeManageAccountsShowConsistencyPromo) {
  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];
  [[delegate expect] onShowConsistencyPromo];

  NSDictionary* headers = [NSDictionary
      dictionaryWithObject:@"action=ADDSESSION,show_consistency_promo=true"
                    forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
  EXPECT_CALL(*account_reconcilor_, OnReceivedManageAccountsResponse(
                                        signin::GAIA_SERVICE_TYPE_ADDSESSION))
      .Times(1);
  EXPECT_TRUE(
      web_state_.ShouldAllowResponse(response, /* for_main_frame = */ true));
  SimulateNavigateToUrl(web::PageLoadCompletionStatus::SUCCESS,
                        GURL("https://accounts.google.com/"));
  web_state_.WebStateDestroyed();

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the consistency promo is not displayed when a page fails to load.
TEST_F(AccountConsistencyServiceTest,
       ChromeManageAccountsNotShowConsistencyPromoOnPageLoadFailure) {
  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];
  [[delegate reject] onShowConsistencyPromo];

  NSDictionary* headers = [NSDictionary
      dictionaryWithObject:@"action=ADDSESSION,show_consistency_promo=true"
                    forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
  EXPECT_CALL(*account_reconcilor_, OnReceivedManageAccountsResponse(
                                        signin::GAIA_SERVICE_TYPE_ADDSESSION))
      .Times(1);
  EXPECT_TRUE(
      web_state_.ShouldAllowResponse(response, /* for_main_frame = */ true));
  SimulateNavigateToUrl(web::PageLoadCompletionStatus::FAILURE,
                        GURL("https://accounts.google.com/"));
  web_state_.WebStateDestroyed();

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the consistency promo is not displayed when a page fails to load
// and user chooses another action.
TEST_F(AccountConsistencyServiceTest,
       ChromeManageAccountsNotShowConsistencyPromoOnPageLoadFailureRedirect) {
  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];
  [[delegate expect] onAddAccount];
  [[delegate reject] onShowConsistencyPromo];

  NSDictionary* headers = [NSDictionary
      dictionaryWithObject:@"action=ADDSESSION,show_consistency_promo=true"
                    forKey:@"X-Chrome-Manage-Accounts"];
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
  EXPECT_CALL(*account_reconcilor_, OnReceivedManageAccountsResponse(
                                        signin::GAIA_SERVICE_TYPE_ADDSESSION))
      .Times(2);

  NSHTTPURLResponse* responseSignin = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];
  EXPECT_TRUE(web_state_.ShouldAllowResponse(responseSignin,
                                             /* for_main_frame = */ true));
  const GURL accountsUrl = GURL("https://accounts.google.com/");
  SimulateNavigateToUrl(web::PageLoadCompletionStatus::FAILURE, accountsUrl);

  NSDictionary* headersAddAccount =
      [NSDictionary dictionaryWithObject:@"action=ADDSESSION"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* responseAddAccount = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headersAddAccount];
  EXPECT_FALSE(web_state_.ShouldAllowResponse(responseAddAccount,
                                              /* for_main_frame = */ true));
  SimulateNavigateToUrl(web::PageLoadCompletionStatus::SUCCESS, accountsUrl);

  web_state_.WebStateDestroyed();

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the consistency promo is not displayed when a non GAIA URL is
// committed.
TEST_F(AccountConsistencyServiceTest,
       ChromeManageAccountsNotShowConsistencyPromoOnNonGaiaURL) {
  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];
  [[delegate reject] onShowConsistencyPromo];

  NSDictionary* headers = [NSDictionary
      dictionaryWithObject:@"action=ADDSESSION,show_consistency_promo=true"
                    forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
  EXPECT_CALL(*account_reconcilor_, OnReceivedManageAccountsResponse(
                                        signin::GAIA_SERVICE_TYPE_ADDSESSION))
      .Times(1);
  EXPECT_TRUE(
      web_state_.ShouldAllowResponse(response, /* for_main_frame = */ true));
  SimulateNavigateToUrl(web::PageLoadCompletionStatus::SUCCESS,
                        GURL("https://youtube.com/"));
  web_state_.WebStateDestroyed();

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the ManageAccountsDelegate is notified when a navigation on Gaia
// signon realm returns with a X-Chrome-Manage-Accounts header with ADDSESSION
// action.
TEST_F(AccountConsistencyServiceTest, ChromeManageAccountsShowAddAccount) {
  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];
  [[delegate expect] onAddAccount];

  NSDictionary* headers =
      [NSDictionary dictionaryWithObject:@"action=ADDSESSION"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
  EXPECT_CALL(*account_reconcilor_, OnReceivedManageAccountsResponse(
                                        signin::GAIA_SERVICE_TYPE_ADDSESSION))
      .Times(1);
  EXPECT_FALSE(
      web_state_.ShouldAllowResponse(response, /* for_main_frame = */ true));
  web_state_.WebStateDestroyed();

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that domains with cookie are correctly loaded from the prefs on service
// startup.
TEST_F(AccountConsistencyServiceTest, DomainsWithCookieLoadedFromPrefs) {
  SignIn();
  CheckDomainHasChromeConnectedCookie(kGoogleDomain);
  CheckDomainHasChromeConnectedCookie(kYoutubeDomain);

  ResetAccountConsistencyService();
  CheckDomainHasChromeConnectedCookie(kGoogleDomain);
  CheckDomainHasChromeConnectedCookie(kYoutubeDomain);

  SignOut();
  CheckNoChromeConnectedCookies();
}

// Tests that domains with cookie are cleared when browsing data is removed.
TEST_F(AccountConsistencyServiceTest, DomainsClearedOnBrowsingDataRemoved) {
  SignIn();
  CheckDomainHasChromeConnectedCookie(kGoogleDomain);
  CheckDomainHasChromeConnectedCookie(kYoutubeDomain);
  EXPECT_EQ(
      2u,
      prefs_.GetDictionary(AccountConsistencyService::kDomainsWithCookiePref)
          ->size());

  // Sets Response to get IdentityManager::Observer::OnAccountsInCookieUpdated
  // through GaiaCookieManagerService::OnCookieChange.
  signin::SetListAccountsResponseNoAccounts(&test_url_loader_factory_);

  base::RunLoop run_loop;
  identity_test_env_->identity_manager_observer()
      ->SetOnAccountsInCookieUpdatedCallback(run_loop.QuitClosure());
  // OnBrowsingDataRemoved triggers
  // AccountsCookieMutator::ForceTriggerOnCookieChange and finally
  // IdentityManager::Observer::OnAccountsInCookieUpdated is called.
  account_consistency_service_->OnBrowsingDataRemoved();
  EXPECT_EQ(
      0u,
      prefs_.GetDictionary(AccountConsistencyService::kDomainsWithCookiePref)
          ->size());
  run_loop.Run();

  // AccountConsistency service is supposed to rebuild the CHROME_CONNECTED
  // cookies when browsing data is removed.
  CheckDomainHasChromeConnectedCookie(kGoogleDomain);
  CheckDomainHasChromeConnectedCookie(kYoutubeDomain);
  EXPECT_EQ(
      2u,
      prefs_.GetDictionary(AccountConsistencyService::kDomainsWithCookiePref)
          ->size());
}

TEST_F(AccountConsistencyServiceTest, SetChromeConnectedCookieNotUpdateTime) {
  SignIn();

  const base::Time signin_time = base::Time::Now();
  // Advance clock before 24-hour CHROME_CONNECTED update time.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(2));
  SimulateSetChromeConnectedCookieForGoogleDomain();

  EXPECT_EQ(signin_time, GetCookieLastUpdateTime(kGoogleDomain));
}

TEST_F(AccountConsistencyServiceTest, SetChromeConnectedCookieAtUpdateTime) {
  SignIn();

  // Advance clock past 24-hour CHROME_CONNECTED update time.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(2));
  const base::Time second_cookie_update_time = base::Time::Now();
  SimulateSetChromeConnectedCookieForGoogleDomain();

  EXPECT_EQ(second_cookie_update_time, GetCookieLastUpdateTime(kGoogleDomain));
}

TEST_F(AccountConsistencyServiceTest, SetGaiaCookieUpdateNotUpdateTime) {
  SignIn();
  SimulateUpdateGaiaCookie();

  // Advance clock, but stay within the one-hour Gaia update time.
  const base::Time first_update_time = base::Time::Now();
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(1));
  SimulateUpdateGaiaCookie();

  EXPECT_EQ(first_update_time, GetGaiaLastUpdateTime());
}

TEST_F(AccountConsistencyServiceTest, SetGaiaCookieUpdateAtUpdateTime) {
  SignIn();
  SimulateUpdateGaiaCookie();

  // Advance clock past one-hour Gaia update time.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(2));
  const base::Time second_update_time = base::Time::Now();
  SimulateUpdateGaiaCookie();

  EXPECT_EQ(second_update_time, GetGaiaLastUpdateTime());
}

// Ensures that the presence or absence of GAIA cookies is logged even if the
// |kRestoreGAIACookiesIfDeleted| experiment is disabled.
TEST_F(AccountConsistencyServiceTest, GAIACookieStatusLoggedProperly) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectTotalCount(kGAIACookiePresentHistogram, 0);
  SimulateUpdateGaiaCookie();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kGAIACookiePresentHistogram, 0);
  SignIn();
  SimulateUpdateGaiaCookie();
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(kGAIACookiePresentHistogram, 1);
}

// Ensures that set and remove cookie operations are handled in the order
// they are called resulting in no cookies.
TEST_F(AccountConsistencyServiceTest, DeleteChromeConnectedCookiesAfterSet) {
  SignIn();

  // URLs must pass IsGoogleDomainURL and not duplicate sign-in domains
  // |kGoogleDomain| or |kYouTubeDomain| otherwise they will not be reset since
  // it is before the update time. Add multiple URLs to test for race conditions
  // with remove call.
  account_consistency_service_->SetChromeConnectedCookieWithUrls(
      {GURL("https://google.ca"), GURL("https://google.fr"),
       GURL("https://google.de")});
  RemoveAllChromeConnectedCookies();

  WaitUntilAllCookieRequestsAreApplied();
  CheckNoChromeConnectedCookies();
}

// Ensures that set and remove cookie operations are handled in the order
// they are called resulting in one cookie.
TEST_F(AccountConsistencyServiceTest, SetChromeConnectedCookiesAfterDelete) {
  SignIn();

  // URLs must pass IsGoogleDomainURL and not duplicate sign-in domains
  // |kGoogleDomain| or |kYouTubeDomain| otherwise they will not be reset since
  // it is before the update time. Add multiple URLs to test for race conditions
  // with remove call.
  account_consistency_service_->SetChromeConnectedCookieWithUrls(
      {GURL("https://google.ca"), GURL("https://google.fr"),
       GURL("https://google.de")});
  RemoveAllChromeConnectedCookies();
  account_consistency_service_->SetChromeConnectedCookieWithUrls(
      {GURL("https://google.ca")});

  WaitUntilAllCookieRequestsAreApplied();
  CheckDomainHasChromeConnectedCookie("google.ca");
}

// Ensures that CHROME_CONNECTED cookies are not set on google.com when the user
// is signed out and navigating to google.com for |kMobileIdentityConsistency|
// experiment.
TEST_F(AccountConsistencyServiceTest,
       SetMiceChromeConnectedCookiesSignedOutGoogleVisitor) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(signin::kMobileIdentityConsistency);
  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];

  NSDictionary* headers =
      [NSDictionary dictionaryWithObject:@"action=ADDSESSION"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);

  EXPECT_TRUE(web_state_.ShouldAllowResponse(response,
                                             /* for_main_frame = */ true));

  CheckNoChromeConnectedCookies();
  SimulateNavigateToUrl(web::PageLoadCompletionStatus::SUCCESS,
                        GURL("https://google.com/"));
  CheckNoChromeConnectedCookies();

  web_state_.WebStateDestroyed();
  EXPECT_OCMOCK_VERIFY(delegate);
}

// Ensures that CHROME_CONNECTED cookies are only set on GAIA urls when the user
// is signed out and taps sign-in button for |kMobileIdentityConsistency|
// experiment. These cookies are immediately removed after the sign-in promo is
// shown.
TEST_F(AccountConsistencyServiceTest,
       SetMiceChromeConnectedCookiesSignedOutGaiaVisitor) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(signin::kMobileIdentityConsistency);
  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];
  [[delegate expect] onShowConsistencyPromo];

  NSDictionary* headers = [NSDictionary
      dictionaryWithObject:@"action=ADDSESSION,show_consistency_promo=true"
                    forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);

  EXPECT_TRUE(web_state_.ShouldAllowResponse(response,
                                             /* for_main_frame = */ true));

  CheckDomainHasChromeConnectedCookie("accounts.google.com");
  SimulateNavigateToUrl(web::PageLoadCompletionStatus::SUCCESS,
                        GURL("https://accounts.google.com/"));
  CheckNoChromeConnectedCookies();

  web_state_.WebStateDestroyed();
  EXPECT_OCMOCK_VERIFY(delegate);
}
