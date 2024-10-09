// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/ios/browser/account_consistency_service.h"

#import <WebKit/WebKit.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/account_reconcilor_delegate.h"
#include "components/signin/core/browser/chrome_connected_header_helper.h"
#import "components/signin/ios/browser/manage_accounts_delegate.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "ios/web/public/navigation/web_state_policy_decider.h"
#include "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "net/base/apple/url_conversions.h"
#include "net/cookies/cookie_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

using testing::NiceMock;

namespace {
// Fake identity email.
const char* kFakeEmail = "janedoe@gmail.com";
// Google domain.
const char* kGoogleDomain = "google.com";
// Youtube domain.
const char* kYoutubeDomain = "youtube.com";
// Google domain where the CHROME_CONNECTED cookie is set/removed.
const char* kCountryGoogleDomain = "google.de";

// Name of the histogram to record the state of the GAIA cookie for the
// navigation.
const char* kGAIACookieOnNavigationHistogram =
    "Signin.IOSGaiaCookieStateOnSignedInNavigation";

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

// Fake delegate implementation; all it does it count delegate calls.
class FakeManageAccountsDelegate : public ManageAccountsDelegate {
 public:
  FakeManageAccountsDelegate() = default;
  ~FakeManageAccountsDelegate() override = default;

  void OnRestoreGaiaCookies() override { restore_cookies_call_count_++; }
  void OnManageAccounts() override { manage_accounts_call_count_++; }
  void OnAddAccount() override { add_account_call_count_++; }
  void OnShowConsistencyPromo(const GURL& url,
                              web::WebState* webState) override {
    show_promo_call_count_++;
  }
  void OnGoIncognito(const GURL& url) override { go_incognito_call_count_++; }

  int total_call_count() {
    return restore_cookies_call_count_ + manage_accounts_call_count_ +
           add_account_call_count_ + show_promo_call_count_ +
           go_incognito_call_count_;
  }

  int restore_cookies_call_count_ = 0;
  int manage_accounts_call_count_ = 0;
  int add_account_call_count_ = 0;
  int show_promo_call_count_ = 0;
  int go_incognito_call_count_ = 0;
};

// FakeWebState that allows control over its policy decider.
class FakeWebState : public web::FakeWebState {
 public:
  FakeWebState() : web::FakeWebState(), decider_(nullptr) {}
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
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policyDecision = decision;
        });
    web::WebStatePolicyDecider::ResponseInfo response_info(for_main_frame);
    decider_->ShouldAllowResponse(response, response_info, std::move(callback));
    return policyDecision.ShouldAllowNavigation();
  }
  void WebStateDestroyed() {
    if (!decider_)
      return;
    decider_->WebStateDestroyed();
  }

 private:
  raw_ptr<web::WebStatePolicyDecider> decider_;
};

}  // namespace

class AccountConsistencyServiceTest : public PlatformTest {
 public:
  AccountConsistencyServiceTest() = default;

 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    content_settings::CookieSettings::RegisterProfilePrefs(prefs_.registry());
    HostContentSettingsMap::RegisterProfilePrefs(prefs_.registry());

    signin_client_.reset(
        new TestSigninClient(&prefs_, &test_url_loader_factory_));
    identity_test_env_.reset(new signin::IdentityTestEnvironment(
        /*test_url_loader_factory=*/nullptr, &prefs_, signin_client_.get()));
    settings_map_ = new HostContentSettingsMap(
        &prefs_, false /* is_off_the_record */, false /* store_last_modified */,
        false /* restore_session */, false /* should_record_metrics */);
    cookie_settings_ = new content_settings::CookieSettings(
        settings_map_.get(), &prefs_, /*tracking_protection_settings=*/nullptr,
        false,
        content_settings::CookieSettings::NoFedCmSharingPermissionsCallback(),
        /*tpcd_metadata_manager=*/nullptr, "");
    // Use a NiceMock here to suppress "uninteresting call" warnings.
    account_reconcilor_ =
        std::make_unique<NiceMock<MockAccountReconcilor>>(signin_client_.get());
    ResetAccountConsistencyService();
  }

  void TearDown() override {
    if (has_set_web_state_handler_) {
      account_consistency_service_->RemoveWebStateHandler(&web_state_);
      has_set_web_state_handler_ = false;
    }

    // Destroy the web state before shutting down
    // |account_consistency_service_|.
    web_state_.WebStateDestroyed();

    account_consistency_service_->Shutdown();
    settings_map_->ShutdownOnUIThread();
    account_reconcilor_->Shutdown();
    identity_test_env_.reset();
    PlatformTest::TearDown();
  }

  void ResetAccountConsistencyService() {
    if (account_consistency_service_) {
      if (has_set_web_state_handler_) {
        account_consistency_service_->RemoveWebStateHandler(&web_state_);
        has_set_web_state_handler_ = false;
      }
      account_consistency_service_->Shutdown();
    }
    // base::Unretained(...) is safe since the AccountConsistencyService does
    // not outlive the BrowserState.
    auto cookie_manager_callback =
        base::BindRepeating(&web::BrowserState::GetCookieManager,
                            base::Unretained(&browser_state_));

    account_consistency_service_ = std::make_unique<AccountConsistencyService>(
        std::move(cookie_manager_callback), account_reconcilor_.get(),
        cookie_settings_, identity_test_env_->identity_manager());
  }

  // Identity APIs.
  void SignIn() {
    signin::MakePrimaryAccountAvailable(identity_test_env_->identity_manager(),
                                        kFakeEmail,
                                        signin::ConsentLevel::kSync);
    WaitUntilAllCookieRequestsAreApplied();
  }

  void SignOut() {
    signin::ClearPrimaryAccount(identity_test_env_->identity_manager());
    WaitUntilAllCookieRequestsAreApplied();
  }

  // Cookie verification APIs.
  void CheckDomainHasChromeConnectedCookie(const std::string& domain) {
    EXPECT_TRUE(ContainsCookie(GetCookiesInCookieJar(),
                               signin::kChromeConnectedCookieName,
                               GetCookieDomain(domain)));
  }

  void CheckNoChromeConnectedCookieForDomain(const std::string& domain) {
    EXPECT_FALSE(ContainsCookie(GetCookiesInCookieJar(),
                                signin::kChromeConnectedCookieName,
                                GetCookieDomain(domain)));
  }

  void CheckNoChromeConnectedCookies() {
    EXPECT_FALSE(ContainsCookie(GetCookiesInCookieJar(),
                                signin::kChromeConnectedCookieName,
                                /*domain=*/std::string()));
  }

  // Verifies the time that the Gaia cookie was last updated for google.com.
  void CheckGaiaCookieWithUpdateTime(base::Time time) {
    EXPECT_EQ(time,
              account_consistency_service_->last_gaia_cookie_update_time_);
  }

  // Navigation APIs.
  void SimulateNavigateToURL(NSURLResponse* response,
                             ManageAccountsDelegate* delegate) {
    SimulateNavigateToURL(response, delegate,
                          web::PageLoadCompletionStatus::SUCCESS,
                          /* expected_allowed_response=*/true);
  }

  void SimulateNavigateToURLWithPageLoadFailure(
      NSURLResponse* response,
      ManageAccountsDelegate* delegate) {
    SimulateNavigateToURL(response, delegate,
                          web::PageLoadCompletionStatus::FAILURE,
                          /* expected_allowed_response=*/true);
  }

  void SimulateNavigateToURLWithInterruption(NSURLResponse* response,
                                             ManageAccountsDelegate* delegate) {
    SimulateNavigateToURL(response, delegate,
                          web::PageLoadCompletionStatus::SUCCESS,
                          /* expected_allowed_response=*/false);
  }

  // Cookie APIs.
  void WaitUntilAllCookieRequestsAreApplied() {
    // Spinning the runloop is needed to ensure that the cookie manager requests
    // are executed.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(0, account_consistency_service_
                     ->active_cookie_manager_requests_for_testing_);
  }

  // Simulate the action of GaiaCookieManagerService to cleanup the cookies
  // once the sign-out is done.
  void RemoveAllChromeConnectedCookies() {
    base::RunLoop run_loop;
    account_consistency_service_->RemoveAllChromeConnectedCookies(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Simulate removing all cookies associated with the google.com domain through
  // an external source.
  void SimulateExternalSourceRemovesAllGoogleDomainCookies() {
    network::mojom::CookieManager* cookie_manager =
        browser_state_.GetCookieManager();
    network::mojom::CookieDeletionFilterPtr filter =
        network::mojom::CookieDeletionFilter::New();
    filter->including_domains =
        std::optional<std::vector<std::string>>({kGoogleDomain});
    cookie_manager->DeleteCookies(std::move(filter),
                                  base::OnceCallback<void(uint)>());
  }

  void SetWebStateHandler(ManageAccountsDelegate* delegate) {
    // If we have already added the |web_state_| with a previous |delegate|,
    // remove it to enforce a one-to-one mapping between web state handler and
    // web state.
    if (has_set_web_state_handler_)
      account_consistency_service_->RemoveWebStateHandler(&web_state_);

    account_consistency_service_->SetWebStateHandler(&web_state_, delegate);
    has_set_web_state_handler_ = true;
  }

  // Properties available for tests.
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  web::FakeBrowserState browser_state_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  FakeWebState web_state_;
  FakeManageAccountsDelegate delegate_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<AccountConsistencyService> account_consistency_service_;
  std::unique_ptr<MockAccountReconcilor> account_reconcilor_;

 private:
  void SimulateNavigateToURL(NSURLResponse* response,
                             ManageAccountsDelegate* delegate,
                             web::PageLoadCompletionStatus page_status,
                             bool expect_allowed_response) {
    SetWebStateHandler(delegate);
    EXPECT_EQ(
        expect_allowed_response,
        web_state_.ShouldAllowResponse(response, /* for_main_frame = */ true));

    web_state_.SetCurrentURL(net::GURLWithNSURL(response.URL));
    web_state_.OnPageLoaded(page_status);
  }

  // Returns set of cookies available to the cookie manager.
  std::vector<net::CanonicalCookie> GetCookiesInCookieJar() {
    std::vector<net::CanonicalCookie> cookies_out;
    base::RunLoop run_loop;
    network::mojom::CookieManager* cookie_manager =
        browser_state_.GetCookieManager();
    cookie_manager->GetAllCookies(base::BindLambdaForTesting(
        [&run_loop,
         &cookies_out](const std::vector<net::CanonicalCookie>& cookies) {
          cookies_out = cookies;
          run_loop.Quit();
        }));
    run_loop.Run();

    return cookies_out;
  }

  // Private properties.
  std::unique_ptr<TestSigninClient> signin_client_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  bool has_set_web_state_handler_ = false;
};

// Tests that main domains are added to the internal map when cookies are set in
// reaction to signin.
TEST_F(AccountConsistencyServiceTest, SigninAddCookieOnMainDomains) {
  SignIn();
  CheckDomainHasChromeConnectedCookie(kGoogleDomain);
  CheckDomainHasChromeConnectedCookie(kYoutubeDomain);
}

// Tests that cookies that are added during SignIn and subsequent navigations
// are correctly removed during the SignOut.
TEST_F(AccountConsistencyServiceTest, SignInSignOut) {
  SignIn();
  CheckDomainHasChromeConnectedCookie(kGoogleDomain);
  CheckDomainHasChromeConnectedCookie(kYoutubeDomain);
  CheckNoChromeConnectedCookieForDomain(kCountryGoogleDomain);

  NSDictionary* headers = [NSDictionary dictionary];

  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://google.de/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];

  SimulateNavigateToURL(response, &delegate_);

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
  NSDictionary* headers =
      [NSDictionary dictionaryWithObject:@"action=DEFAULT"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://google.com"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];

  SimulateNavigateToURL(response, &delegate_);
  EXPECT_EQ(0, delegate_.total_call_count());
}

// Tests that navigation to Gaia signon realm with no X-Chrome-Manage-Accounts
// header in the response are simply untouched.
TEST_F(AccountConsistencyServiceTest, ChromeManageAccountsNoHeader) {
  NSDictionary* headers = [NSDictionary dictionary];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];

  SimulateNavigateToURL(response, &delegate_);
  EXPECT_EQ(0, delegate_.total_call_count());
}

// Tests that the ManageAccountsDelegate is notified when a navigation on Gaia
// signon realm returns with a X-Chrome-Manage-Accounts header with action
// DEFAULT.
TEST_F(AccountConsistencyServiceTest, ChromeManageAccountsDefault) {
  NSDictionary* headers =
      [NSDictionary dictionaryWithObject:@"action=DEFAULT"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];
  EXPECT_CALL(*account_reconcilor_, OnReceivedManageAccountsResponse(
                                        signin::GAIA_SERVICE_TYPE_DEFAULT))
      .Times(1);

  SimulateNavigateToURLWithInterruption(response, &delegate_);

  EXPECT_EQ(1, delegate_.total_call_count());
  EXPECT_EQ(1, delegate_.manage_accounts_call_count_);
}

// Tests that the ManageAccountsDelegate is notified when a navigation on Gaia
// signon realm returns with a X-Auto-Login header.
TEST_F(AccountConsistencyServiceTest, ChromeShowConsistencyPromo) {
  NSDictionary* headers = [NSDictionary dictionaryWithObject:@"args=unused"
                                                      forKey:@"X-Auto-Login"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];

  SimulateNavigateToURL(response, &delegate_);

  EXPECT_EQ(1, delegate_.total_call_count());
  EXPECT_EQ(1, delegate_.show_promo_call_count_);
}

// Tests that the consistency promo is not displayed when a page fails to load.
TEST_F(AccountConsistencyServiceTest,
       ChromeNotShowConsistencyPromoOnPageLoadFailure) {
  NSDictionary* headers = [NSDictionary dictionaryWithObject:@"args=unused"
                                                      forKey:@"X-Auto-Login"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];

  SimulateNavigateToURLWithPageLoadFailure(response, &delegate_);
  EXPECT_EQ(0, delegate_.total_call_count());
}

// Tests that the consistency promo is not displayed when a page fails to load
// and user chooses another action.
TEST_F(AccountConsistencyServiceTest,
       ChromeNotShowConsistencyPromoOnPageLoadFailureRedirect) {
  EXPECT_CALL(*account_reconcilor_, OnReceivedManageAccountsResponse(
                                        signin::GAIA_SERVICE_TYPE_ADDSESSION));

  NSDictionary* headers = [NSDictionary dictionaryWithObject:@"args=unused"
                                                      forKey:@"X-Auto-Login"];
  NSHTTPURLResponse* responseSignin = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];

  SimulateNavigateToURLWithPageLoadFailure(responseSignin, &delegate_);

  NSDictionary* headersAddAccount =
      [NSDictionary dictionaryWithObject:@"action=ADDSESSION"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* responseAddAccount = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headersAddAccount];

  SimulateNavigateToURLWithInterruption(responseAddAccount, &delegate_);

  EXPECT_EQ(1, delegate_.total_call_count());
  EXPECT_EQ(1, delegate_.add_account_call_count_);
  EXPECT_EQ(0, delegate_.show_promo_call_count_);
}

// Tests that the ManageAccountsDelegate is notified when a navigation on Gaia
// signon realm returns with a X-Chrome-Manage-Accounts header with ADDSESSION
// action.
TEST_F(AccountConsistencyServiceTest, ChromeManageAccountsShowAddAccount) {
  NSDictionary* headers =
      [NSDictionary dictionaryWithObject:@"action=ADDSESSION"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];
  EXPECT_CALL(*account_reconcilor_, OnReceivedManageAccountsResponse(
                                        signin::GAIA_SERVICE_TYPE_ADDSESSION))
      .Times(1);

  SimulateNavigateToURLWithInterruption(response, &delegate_);
  EXPECT_EQ(1, delegate_.total_call_count());
  EXPECT_EQ(1, delegate_.add_account_call_count_);
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

  run_loop.Run();

  // AccountConsistency service is supposed to rebuild the CHROME_CONNECTED
  // cookies when browsing data is removed.
  CheckDomainHasChromeConnectedCookie(kGoogleDomain);
  CheckDomainHasChromeConnectedCookie(kYoutubeDomain);
}

// Tests that google.com domain cookies can be regenerated after an external
// source removes these cookies.
TEST_F(AccountConsistencyServiceTest,
       AddChromeConnectedCookiesOnCookiesRemoved) {
  SignIn();
  CheckDomainHasChromeConnectedCookie(kGoogleDomain);

  SimulateExternalSourceRemovesAllGoogleDomainCookies();
  CheckNoChromeConnectedCookieForDomain(kGoogleDomain);

  // Forcibly rebuild the CHROME_CONNECTED cookies.
  account_consistency_service_->AddChromeConnectedCookies();

  CheckDomainHasChromeConnectedCookie(kGoogleDomain);
}

// Tests that the CHROME_CONNECTED cookie is set on Google and Google-associated
// domains when the account consistency service runs.
TEST_F(AccountConsistencyServiceTest, SetChromeConnectedCookie) {
  SignIn();

  NSDictionary* headers = [NSDictionary dictionary];

  // HTTP response URL is eligible for Mirror (the test does not use google.com
  // since the CHROME_CONNECTED cookie is generated for it by default.
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://youtube.com"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];

  SimulateNavigateToURL(response, &delegate_);
  SimulateExternalSourceRemovesAllGoogleDomainCookies();

  SimulateNavigateToURL(response, &delegate_);

  CheckDomainHasChromeConnectedCookie(kGoogleDomain);
  CheckDomainHasChromeConnectedCookie(kYoutubeDomain);
}

// Tests that navigating to accounts.google.com without a GAIA cookie is logged
// by the navigation histogram.
TEST_F(AccountConsistencyServiceTest, GAIACookieMissingOnSignin) {
  SignIn();

  NSDictionary* headers =
      [NSDictionary dictionaryWithObject:@"action=ADDSESSION"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];
  EXPECT_CALL(*account_reconcilor_, OnReceivedManageAccountsResponse(
                                        signin::GAIA_SERVICE_TYPE_ADDSESSION))
      .Times(2);

  SimulateNavigateToURL(response, &delegate_);
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kGAIACookieOnNavigationHistogram, 0);

  SimulateExternalSourceRemovesAllGoogleDomainCookies();

  // Gaia cookie is not restored due to one-hour time restriction.
  SimulateNavigateToURLWithInterruption(response, &delegate_);
  histogram_tester.ExpectTotalCount(kGAIACookieOnNavigationHistogram, 1);

  EXPECT_EQ(1, delegate_.total_call_count());
  EXPECT_EQ(1, delegate_.add_account_call_count_);
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
// is signed out and navigating to google.com.
TEST_F(AccountConsistencyServiceTest,
       SetChromeConnectedCookiesSignedOutGoogleVisitor) {
  NSDictionary* headers =
      [NSDictionary dictionaryWithObject:@"action=ADDSESSION"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];

  CheckNoChromeConnectedCookies();

  SimulateNavigateToURL(response, &delegate_);

  CheckNoChromeConnectedCookies();
  EXPECT_EQ(0, delegate_.total_call_count());
}

// Ensures that CHROME_CONNECTED cookies are not set when the user is signed out
// after the sign-in promo is shown.
TEST_F(AccountConsistencyServiceTest,
       SetChromeConnectedCookiesSignedOutGaiaVisitor) {
  NSDictionary* headers = [NSDictionary dictionaryWithObject:@"args=unused"
                                                      forKey:@"X-Auto-Login"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];

  SetWebStateHandler(&delegate_);
  EXPECT_TRUE(web_state_.ShouldAllowResponse(response,
                                             /* for_main_frame = */ true));

  web_state_.SetCurrentURL(net::GURLWithNSURL(response.URL));
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  CheckNoChromeConnectedCookies();
  EXPECT_EQ(1, delegate_.total_call_count());
  EXPECT_EQ(1, delegate_.show_promo_call_count_);
}

TEST_F(AccountConsistencyServiceTest, SetGaiaCookieUpdateBeforeDelay) {
  SignIn();

  NSDictionary* headers =
      [NSDictionary dictionaryWithObject:@"action=ADDSESSION"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];

  SimulateNavigateToURL(response, nil);

  // Advance clock, but stay within the one-hour Gaia update time.
  base::TimeDelta oneMinuteDelta = base::Minutes(1);
  task_environment_.FastForwardBy(oneMinuteDelta);
  SimulateNavigateToURLWithInterruption(response, nullptr);

  // Does not process the second Gaia restore event.
  CheckGaiaCookieWithUpdateTime(base::Time::Now() - oneMinuteDelta);
}

TEST_F(AccountConsistencyServiceTest, SetGaiaCookieUpdateAfterDelay) {
  SignIn();

  NSDictionary* headers =
      [NSDictionary dictionaryWithObject:@"action=ADDSESSION"
                                  forKey:@"X-Chrome-Manage-Accounts"];
  NSHTTPURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@"https://accounts.google.com/"]
        statusCode:200
       HTTPVersion:@"HTTP/1.1"
      headerFields:headers];

  SimulateNavigateToURL(response, nil);

  // Advance clock past the one-hour Gaia update time.
  base::TimeDelta twoHourDelta = base::Hours(2);
  task_environment_.FastForwardBy(twoHourDelta);
  SimulateNavigateToURL(response, nil);

  // Will process the second Gaia restore event, since it is past the delay.
  CheckGaiaCookieWithUpdateTime(base::Time::Now());
}
