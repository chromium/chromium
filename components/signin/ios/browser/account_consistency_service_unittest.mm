// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/account_consistency_service.h"

#import <WebKit/WebKit.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/gaia_constants.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/web_state/web_state_policy_decider.h"
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

// AccountConsistencyService specialization that fakes the creation of the
// WKWebView in order to mock it. This allows tests to intercept the calls to
// the Web view and control they are correct.
class FakeAccountConsistencyService : public AccountConsistencyService {
 public:
  FakeAccountConsistencyService(
      web::BrowserState* browser_state,
      AccountReconcilor* account_reconcilor,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      GaiaCookieManagerService* gaia_cookie_manager_service,
      SigninClient* signin_client,
      SigninManager* signin_manager)
      : AccountConsistencyService(browser_state,
                                  account_reconcilor,
                                  cookie_settings,
                                  gaia_cookie_manager_service,
                                  signin_client,
                                  signin_manager) {}

 private:
  WKWebView* BuildWKWebView() override {
    if (!mock_web_view_) {
      mock_web_view_ = [OCMockObject niceMockForClass:[WKWebView class]];
    }
    return mock_web_view_;
  }
  id mock_web_view_;
};

// Mock AccountReconcilor to catch call to OnReceivedManageAccountsResponse.
class MockAccountReconcilor : public AccountReconcilor {
 public:
  MockAccountReconcilor(SigninClient* client)
      : AccountReconcilor(
            nullptr,
            nullptr,
            client,
            nullptr,
            std::make_unique<signin::AccountReconcilorDelegate>()) {}
  MOCK_METHOD1(OnReceivedManageAccountsResponse, void(signin::GAIAServiceType));
};

// Mock GaiaCookieManagerService to catch call to ForceOnCookieChangeProcessing
class MockGaiaCookieManagerService : public GaiaCookieManagerService {
 public:
  MockGaiaCookieManagerService()
      : GaiaCookieManagerService(nullptr,
                                 GaiaConstants::kChromeSource,
                                 nullptr) {}
  MOCK_METHOD0(ForceOnCookieChangeProcessing, void());
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
    return decider_ ? decider_->ShouldAllowResponse(response, for_main_frame)
                    : true;
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
  void OnRemoveChromeConnectedCookieFinished() {
    EXPECT_FALSE(remove_cookie_callback_called_);
    EXPECT_EQ(0, web_view_load_expection_count_);
    remove_cookie_callback_called_ = true;
  }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ActiveStateManager::FromBrowserState(&browser_state_)->SetActive(true);
    AccountConsistencyService::RegisterPrefs(prefs_.registry());
    AccountTrackerService::RegisterPrefs(prefs_.registry());
    ProfileOAuth2TokenService::RegisterProfilePrefs(prefs_.registry());
    content_settings::CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());
    SigninManagerBase::RegisterProfilePrefs(prefs_.registry());

    web_view_load_expection_count_ = 0;
    gaia_cookie_manager_service_.reset(new MockGaiaCookieManagerService());
    signin_client_.reset(new TestSigninClient(&prefs_));
    account_tracker_service_.Initialize(&prefs_, base::FilePath());
    token_service_.reset(new FakeProfileOAuth2TokenService(&prefs_));
    signin_manager_.reset(
        new FakeSigninManager(signin_client_.get(), token_service_.get(),
                              &account_tracker_service_, nullptr));
    signin_manager_->Initialize(nullptr);
    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* incognito_profile */, false /* guest_profile */,
        false /* store_last_modified */,
        false /* migrate_requesting_and_top_level_origin_settings */);
    cookie_settings_ =
        new content_settings::CookieSettings(settings_map_.get(), &prefs_, "");
    account_reconcilor_ =
        std::make_unique<MockAccountReconcilor>(signin_client_.get());
    ResetAccountConsistencyService();
  }

  void TearDown() override {
    EXPECT_EQ(0, web_view_load_expection_count_);
    EXPECT_OCMOCK_VERIFY(GetMockWKWebView());
    account_consistency_service_->Shutdown();
    settings_map_->ShutdownOnUIThread();
    ActiveStateManager::FromBrowserState(&browser_state_)->SetActive(false);
    PlatformTest::TearDown();
  }

  // Adds an expectation for |url| to be loaded in the web view of
  // |account_consistency_service_|.
  // |continue_navigation| controls whether navigation will continue or be
  // stopped on page load.
  void AddPageLoadedExpectation(NSURL* url, bool continue_navigation) {
    ++web_view_load_expection_count_;
    void (^continueBlock)(NSInvocation*) = ^(NSInvocation* invocation) {
      --web_view_load_expection_count_;
      if (!continue_navigation)
        return;
      __unsafe_unretained WKWebView* web_view = nil;
      [invocation getArgument:&web_view atIndex:0];
      [GetNavigationDelegate() webView:web_view didFinishNavigation:nil];
    };
    [static_cast<WKWebView*>([[GetMockWKWebView() expect] andDo:continueBlock])
        loadHTMLString:[OCMArg any]
               baseURL:url];
  }

  void ResetAccountConsistencyService() {
    if (account_consistency_service_) {
      account_consistency_service_->Shutdown();
    }
    account_consistency_service_.reset(new FakeAccountConsistencyService(
        &browser_state_, account_reconcilor_.get(), cookie_settings_,
        gaia_cookie_manager_service_.get(), signin_client_.get(),
        signin_manager_.get()));
  }

  void SignIn() {
    signin_manager_->SignIn("12345", "user@gmail.com", "password");
    EXPECT_EQ(0, web_view_load_expection_count_);
  }

  void SignOutAndSimulateGaiaCookieManagerServiceLogout() {
    signin_manager_->ForceSignOut();
    SimulateGaiaCookieManagerServiceLogout(true);
  }

  id GetWKWebView() { return account_consistency_service_->GetWKWebView(); }

  id GetMockWKWebView() {
    // Should use BuildWKWebView() to always have the mock instance, even when
    // the |account_consistency_service_| is inactive.
    return account_consistency_service_->BuildWKWebView();
  }

  id GetNavigationDelegate() {
    return account_consistency_service_->navigation_delegate_;
  }

  bool ShouldAddCookieToDomain(const std::string& domain,
                               bool should_check_last_update_time) {
    return account_consistency_service_->ShouldAddChromeConnectedCookieToDomain(
        domain, should_check_last_update_time);
  }

  void CheckDomainHasCookie(const std::string& domain) {
    EXPECT_GE(
        account_consistency_service_->last_cookie_update_map_.count(domain),
        1u);
  }

  void SimulateGaiaCookieManagerServiceLogout(
      bool expect_remove_cookie_callback) {
    // Simulate the action of the action GaiaCookieManagerService to cleanup
    // the cookies once the sign-out is done.
    remove_cookie_callback_called_ = false;
    account_consistency_service_->RemoveChromeConnectedCookies(base::BindOnce(
        &AccountConsistencyServiceTest::OnRemoveChromeConnectedCookieFinished,
        base::Unretained(this)));
    EXPECT_EQ(expect_remove_cookie_callback, remove_cookie_callback_called_);
  }

  // Creates test threads, necessary for ActiveStateManager that needs a UI
  // thread.
  web::TestWebThreadBundle thread_bundle_;
  AccountTrackerService account_tracker_service_;
  web::TestBrowserState browser_state_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  TestWebState web_state_;
  // AccountConsistencyService being tested. Actually a
  // FakeAccountConsistencyService to be able to use a mock web view.
  std::unique_ptr<AccountConsistencyService> account_consistency_service_;
  std::unique_ptr<TestSigninClient> signin_client_;
  std::unique_ptr<FakeProfileOAuth2TokenService> token_service_;
  std::unique_ptr<FakeSigninManager> signin_manager_;
  std::unique_ptr<MockGaiaCookieManagerService> gaia_cookie_manager_service_;
  std::unique_ptr<MockAccountReconcilor> account_reconcilor_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  bool remove_cookie_callback_called_;
  int web_view_load_expection_count_;
};

// Tests whether the WKWebView is actually stopped when the browser state is
// inactive.
TEST_F(AccountConsistencyServiceTest, OnInactive) {
  [[GetMockWKWebView() expect] stopLoading];
  // Loads the webview.
  EXPECT_TRUE(GetWKWebView());
  ActiveStateManager::FromBrowserState(&browser_state_)->SetActive(false);
}

// Tests that cookies that are added during SignIn and subsequent navigations
// are correctly removed during the SignOut.
TEST_F(AccountConsistencyServiceTest, SignInSignOut) {
  // Check that main Google domains are added.
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignIn();
  // Check that other Google domains are added on navigation.
  AddPageLoadedExpectation(kCountryGoogleUrl, true /* continue_navigation */);

  id delegate =
      [OCMockObject mockForProtocol:@protocol(ManageAccountsDelegate)];
  NSDictionary* headers = [NSDictionary dictionary];
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:kCountryGoogleUrl
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:headers];
  web_view_load_expection_count_ = 1;
  account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
  EXPECT_TRUE(
      web_state_.ShouldAllowResponse(response, /* for_main_frame = */ true));
  web_state_.WebStateDestroyed();
  EXPECT_EQ(0, web_view_load_expection_count_);

  // Check that all domains are removed.
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kCountryGoogleUrl, true /* continue_navigation */);
  SignOutAndSimulateGaiaCookieManagerServiceLogout();
}

// Tests that signing out with no domains known, still call the callback.
TEST_F(AccountConsistencyServiceTest, SignOutWithoutDomains) {
  SignOutAndSimulateGaiaCookieManagerServiceLogout();
}

// Tests that pending cookie requests are correctly applied when the browser
// state becomes active.
TEST_F(AccountConsistencyServiceTest, ApplyOnActive) {
  // No request is made until the browser state is active, then a WKWebView and
  // its navigation delegate are created, and the requests are processed.
  [[GetMockWKWebView() expect] setNavigationDelegate:[OCMArg isNotNil]];
  ActiveStateManager::FromBrowserState(&browser_state_)->SetActive(false);
  SignIn();
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  ActiveStateManager::FromBrowserState(&browser_state_)->SetActive(true);
}

// Tests that cookie request being processed is correctly cancelled when the
// browser state becomes inactives and correctly re-started later when the
// browser state becomes active.
TEST_F(AccountConsistencyServiceTest, CancelOnInactiveReApplyOnActive) {
  // The first request starts to get applied and get cancelled as the browser
  // state becomes inactive. It is resumed after the browser state becomes
  // active again.
  AddPageLoadedExpectation(kGoogleUrl, false /* continue_navigation */);
  SignIn();
  ActiveStateManager::FromBrowserState(&browser_state_)->SetActive(false);
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  ActiveStateManager::FromBrowserState(&browser_state_)->SetActive(true);
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

// Tests that domains with cookie are added to the prefs only after the request
// has been applied.
TEST_F(AccountConsistencyServiceTest, DomainsWithCookiePrefsOnApplied) {
  // Second request is not completely applied. Ensure prefs reflect that.
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, false /* continue_navigation */);
  SignIn();

  const base::DictionaryValue* dict =
      prefs_.GetDictionary(AccountConsistencyService::kDomainsWithCookiePref);
  EXPECT_EQ(1u, dict->size());
  EXPECT_TRUE(dict->GetBooleanWithoutPathExpansion("google.com", nullptr));
  EXPECT_FALSE(dict->GetBooleanWithoutPathExpansion("youtube.com", nullptr));
}

// Tests that domains with cookie are correctly loaded from the prefs on service
// startup.
TEST_F(AccountConsistencyServiceTest, DomainsWithCookieLoadedFromPrefs) {
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignIn();

  ResetAccountConsistencyService();
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignOutAndSimulateGaiaCookieManagerServiceLogout();
}

// Tests that domains with cookie are cleared when browsing data is removed.
TEST_F(AccountConsistencyServiceTest, DomainsClearedOnBrowsingDataRemoved) {
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignIn();
  const base::DictionaryValue* dict =
      prefs_.GetDictionary(AccountConsistencyService::kDomainsWithCookiePref);
  EXPECT_EQ(2u, dict->size());

  EXPECT_CALL(*gaia_cookie_manager_service_, ForceOnCookieChangeProcessing())
      .Times(1);
  account_consistency_service_->OnBrowsingDataRemoved();
  dict =
      prefs_.GetDictionary(AccountConsistencyService::kDomainsWithCookiePref);
  EXPECT_EQ(0u, dict->size());
}

// Tests that remove cookie call back is called when the signout is interrupted
// by removing the browser data.
TEST_F(AccountConsistencyServiceTest, DomainsClearedOnBrowsingDataRemoved2) {
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignIn();

  AddPageLoadedExpectation(kGoogleUrl, false /* continue_navigation */);
  SimulateGaiaCookieManagerServiceLogout(false);
  EXPECT_CALL(*gaia_cookie_manager_service_, ForceOnCookieChangeProcessing())
      .Times(1);
  account_consistency_service_->OnBrowsingDataRemoved();
  EXPECT_TRUE(remove_cookie_callback_called_);
}

// Tests that cookie requests are correctly processed or ignored when the update
// time isn't checked.
TEST_F(AccountConsistencyServiceTest, ShouldAddCookieDontCheckUpdateTime) {
  EXPECT_TRUE(ShouldAddCookieToDomain(kGoogleDomain, false));
  EXPECT_TRUE(ShouldAddCookieToDomain(kYoutubeDomain, false));

  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignIn();

  EXPECT_FALSE(ShouldAddCookieToDomain(kGoogleDomain, false));
  EXPECT_FALSE(ShouldAddCookieToDomain(kYoutubeDomain, false));

  ResetAccountConsistencyService();

  EXPECT_FALSE(ShouldAddCookieToDomain(kGoogleDomain, false));
  EXPECT_FALSE(ShouldAddCookieToDomain(kYoutubeDomain, false));
}

// Tests that cookie requests are correctly processed or ignored when the update
// time is checked.
TEST_F(AccountConsistencyServiceTest, ShouldAddCookieCheckUpdateTime) {
  EXPECT_TRUE(ShouldAddCookieToDomain(kGoogleDomain, true));
  EXPECT_TRUE(ShouldAddCookieToDomain(kYoutubeDomain, true));

  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignIn();

  EXPECT_FALSE(ShouldAddCookieToDomain(kGoogleDomain, true));
  EXPECT_FALSE(ShouldAddCookieToDomain(kYoutubeDomain, true));

  ResetAccountConsistencyService();

  EXPECT_TRUE(ShouldAddCookieToDomain(kGoogleDomain, true));
  EXPECT_TRUE(ShouldAddCookieToDomain(kYoutubeDomain, true));
}

// Tests that main domains are added to the internal map when cookies are set in
// reaction to signin.
TEST_F(AccountConsistencyServiceTest, SigninAddCookieOnMainDomains) {
  AddPageLoadedExpectation(kGoogleUrl, true /* continue_navigation */);
  AddPageLoadedExpectation(kYoutubeUrl, true /* continue_navigation */);
  SignIn();

  CheckDomainHasCookie(kGoogleDomain);
  CheckDomainHasCookie(kYoutubeDomain);
}
