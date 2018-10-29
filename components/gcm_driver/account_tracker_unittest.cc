// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/account_tracker.h"

#include <algorithm>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "net/http/http_status_code.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kOAuthURL[] = "https://www.googleapis.com/oauth2/v1/userinfo";
const char kPrimaryAccountEmail[] = "primary_account@example.com";

enum TrackingEventType { SIGN_IN, SIGN_OUT };

std::string AccountKeyToObfuscatedId(const std::string& email) {
  return "obfid-" + email;
}

class TrackingEvent {
 public:
  TrackingEvent(TrackingEventType type,
                const std::string& account_key,
                const std::string& gaia_id)
      : type_(type), account_key_(account_key), gaia_id_(gaia_id) {}

  TrackingEvent(TrackingEventType type, const std::string& account_key)
      : type_(type),
        account_key_(account_key),
        gaia_id_(AccountKeyToObfuscatedId(account_key)) {}

  bool operator==(const TrackingEvent& event) const {
    return type_ == event.type_ && account_key_ == event.account_key_ &&
           gaia_id_ == event.gaia_id_;
  }

  std::string ToString() const {
    const char* typestr = "INVALID";
    switch (type_) {
      case SIGN_IN:
        typestr = " IN";
        break;
      case SIGN_OUT:
        typestr = "OUT";
        break;
    }
    return base::StringPrintf("{ type: %s, email: %s, gaia: %s }", typestr,
                              account_key_.c_str(), gaia_id_.c_str());
  }

 private:
  friend bool CompareByUser(TrackingEvent a, TrackingEvent b);

  TrackingEventType type_;
  std::string account_key_;
  std::string gaia_id_;
};

bool CompareByUser(TrackingEvent a, TrackingEvent b) {
  return a.account_key_ < b.account_key_;
}

std::string Str(const std::vector<TrackingEvent>& events) {
  std::string str = "[";
  bool needs_comma = false;
  for (auto it = events.begin(); it != events.end(); ++it) {
    if (needs_comma)
      str += ",\n ";
    needs_comma = true;
    str += it->ToString();
  }
  str += "]";
  return str;
}

}  // namespace

namespace gcm {

class AccountTrackerObserver : public AccountTracker::Observer {
 public:
  AccountTrackerObserver() {}
  virtual ~AccountTrackerObserver() {}

  testing::AssertionResult CheckEvents();
  testing::AssertionResult CheckEvents(const TrackingEvent& e1);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2,
                                       const TrackingEvent& e3);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2,
                                       const TrackingEvent& e3,
                                       const TrackingEvent& e4);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2,
                                       const TrackingEvent& e3,
                                       const TrackingEvent& e4,
                                       const TrackingEvent& e5);
  testing::AssertionResult CheckEvents(const TrackingEvent& e1,
                                       const TrackingEvent& e2,
                                       const TrackingEvent& e3,
                                       const TrackingEvent& e4,
                                       const TrackingEvent& e5,
                                       const TrackingEvent& e6);
  void Clear();
  void SortEventsByUser();

  // AccountTracker::Observer implementation
  void OnAccountSignInChanged(const AccountIds& ids,
                              bool is_signed_in) override;

 private:
  testing::AssertionResult CheckEvents(
      const std::vector<TrackingEvent>& events);

  std::vector<TrackingEvent> events_;
};

void AccountTrackerObserver::OnAccountSignInChanged(const AccountIds& ids,
                                                    bool is_signed_in) {
  events_.push_back(
      TrackingEvent(is_signed_in ? SIGN_IN : SIGN_OUT, ids.email, ids.gaia));
}

void AccountTrackerObserver::Clear() {
  events_.clear();
}

void AccountTrackerObserver::SortEventsByUser() {
  std::stable_sort(events_.begin(), events_.end(), CompareByUser);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents() {
  std::vector<TrackingEvent> events;
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2,
    const TrackingEvent& e3) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  events.push_back(e3);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2,
    const TrackingEvent& e3,
    const TrackingEvent& e4) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  events.push_back(e3);
  events.push_back(e4);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2,
    const TrackingEvent& e3,
    const TrackingEvent& e4,
    const TrackingEvent& e5) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  events.push_back(e3);
  events.push_back(e4);
  events.push_back(e5);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const TrackingEvent& e1,
    const TrackingEvent& e2,
    const TrackingEvent& e3,
    const TrackingEvent& e4,
    const TrackingEvent& e5,
    const TrackingEvent& e6) {
  std::vector<TrackingEvent> events;
  events.push_back(e1);
  events.push_back(e2);
  events.push_back(e3);
  events.push_back(e4);
  events.push_back(e5);
  events.push_back(e6);
  return CheckEvents(events);
}

testing::AssertionResult AccountTrackerObserver::CheckEvents(
    const std::vector<TrackingEvent>& events) {
  std::string maybe_newline = (events.size() + events_.size()) > 2 ? "\n" : "";
  testing::AssertionResult result(
      (events_ == events)
          ? testing::AssertionSuccess()
          : (testing::AssertionFailure()
             << "Expected " << maybe_newline << Str(events) << ", "
             << maybe_newline << "Got " << maybe_newline << Str(events_)));
  events_.clear();
  return result;
}

class AccountTrackerTest : public testing::Test {
 public:
  AccountTrackerTest() {}

  ~AccountTrackerTest() override {}

  void SetUp() override {
    account_tracker_.reset(new AccountTracker(
        identity_test_env_.identity_manager(),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_)));
    account_tracker_->AddObserver(&observer_);
  }

  void TearDown() override {
    account_tracker_->RemoveObserver(&observer_);
    account_tracker_->Shutdown();
  }

  AccountTrackerObserver* observer() { return &observer_; }

  AccountTracker* account_tracker() { return account_tracker_.get(); }

  // Helpers to pass fake events to the tracker.

  // Sets the primary account info. Returns the account ID of the
  // newly-set account.
  // NOTE: On ChromeOS, the login callback is never fired in production (since
  // the underlying GoogleSigninSucceeded callback is never sent). Tests that
  // exercise functionality dependent on that callback firing are not relevant
  // on ChromeOS and should simply not run on that platform.
  std::string SetActiveAccount(const std::string& email) {
    return identity_test_env_.SetPrimaryAccount(email).account_id;
  }

// Helpers that go through a logout flow.
// NOTE: On ChromeOS, the logout callback is never fired in production (since
// the underlying GoogleSignedOut callback is never sent). Tests that exercise
// functionality dependent on that callback firing are not relevant on ChromeOS
// and should simply not run on that platform.
#if !defined(OS_CHROMEOS)
  void NotifyLogoutOfPrimaryAccountOnly() {
    identity_test_env_.ClearPrimaryAccount(
        identity::ClearPrimaryAccountPolicy::KEEP_ALL_ACCOUNTS);
  }

  void NotifyLogoutOfAllAccounts() {
    identity_test_env_.ClearPrimaryAccount(
        identity::ClearPrimaryAccountPolicy::REMOVE_ALL_ACCOUNTS);
  }
#endif

  std::string AddAccountWithToken(const std::string& email) {
    return identity_test_env_.MakeAccountAvailable(email).account_id;
  }

  void NotifyTokenAvailable(const std::string& account_id) {
    identity_test_env_.SetRefreshTokenForAccount(account_id);
  }

  void NotifyTokenRevoked(const std::string& account_id) {
    identity_test_env_.RemoveRefreshTokenForAccount(account_id);
  }

  // Helpers to fake access token and user info fetching
  void IssueAccessToken(const std::string& account_id) {
    identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        account_id, "access_token-" + account_id, base::Time::Max());
  }

  std::string GetValidTokenInfoResponse(const std::string& account_id) {
    return std::string("{ \"id\": \"") + AccountKeyToObfuscatedId(account_id) +
           "\" }";
  }

  void ReturnOAuthUrlFetchResults(net::HttpStatusCode response_code,
                                  const std::string& response_string);

  void ReturnOAuthUrlFetchSuccess(const std::string& account_key);
  void ReturnOAuthUrlFetchFailure(const std::string& account_key);

  std::string SetupPrimaryLogin() {
    // Initial setup for tests that start with a signed in profile.
    std::string primary_account_id = SetActiveAccount(kPrimaryAccountEmail);
    NotifyTokenAvailable(primary_account_id);
    ReturnOAuthUrlFetchSuccess(primary_account_id);
    observer()->Clear();

    return primary_account_id;
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  base::MessageLoopForIO message_loop_;  // net:: stuff needs IO message loop.
  network::TestURLLoaderFactory test_url_loader_factory_;
  identity::IdentityTestEnvironment identity_test_env_;

  std::unique_ptr<AccountTracker> account_tracker_;
  AccountTrackerObserver observer_;
};

void AccountTrackerTest::ReturnOAuthUrlFetchResults(
    net::HttpStatusCode response_code,
    const std::string& response_string) {
  EXPECT_TRUE(test_url_loader_factory()->SimulateResponseForPendingRequest(
      GURL(kOAuthURL), network::URLLoaderCompletionStatus(net::OK),
      network::CreateResourceResponseHead(response_code), response_string));
}

void AccountTrackerTest::ReturnOAuthUrlFetchSuccess(
    const std::string& account_id) {
  IssueAccessToken(account_id);
  ReturnOAuthUrlFetchResults(net::HTTP_OK,
                             GetValidTokenInfoResponse(account_id));
}

void AccountTrackerTest::ReturnOAuthUrlFetchFailure(
    const std::string& account_id) {
  IssueAccessToken(account_id);
  ReturnOAuthUrlFetchResults(net::HTTP_BAD_REQUEST, "");
}

// Primary tests just involve the Active account

TEST_F(AccountTrackerTest, PrimaryNoEventsBeforeLogin) {
  std::string account_id = AddAccountWithToken("me@dummy.com");
  NotifyTokenRevoked(account_id);

// Logout is not possible on ChromeOS.
#if !defined(OS_CHROMEOS)
  NotifyLogoutOfAllAccounts();
#endif

  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, PrimaryLoginThenTokenAvailable) {
  std::string primary_account_id = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  EXPECT_TRUE(observer()->CheckEvents());

  ReturnOAuthUrlFetchSuccess(primary_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, primary_account_id)));
}

TEST_F(AccountTrackerTest, PrimaryRevoke) {
  std::string primary_account_id = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  observer()->Clear();

  NotifyTokenRevoked(primary_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, primary_account_id)));
}

TEST_F(AccountTrackerTest, PrimaryRevokeThenTokenAvailable) {
  std::string primary_account_id = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  NotifyTokenRevoked(primary_account_id);
  observer()->Clear();

  NotifyTokenAvailable(primary_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, primary_account_id)));
}

// These tests exercise true login/logout, which are not possible on ChromeOS.
#if !defined(OS_CHROMEOS)
TEST_F(AccountTrackerTest, PrimaryTokenAvailableThenLogin) {
  AddAccountWithToken(kPrimaryAccountEmail);
  EXPECT_TRUE(observer()->CheckEvents());

  std::string primary_account_id = SetActiveAccount(kPrimaryAccountEmail);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, primary_account_id)));
}

TEST_F(AccountTrackerTest, PrimaryTokenAvailableAndRevokedThenLogin) {
  std::string primary_account_id = AddAccountWithToken(kPrimaryAccountEmail);
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyTokenRevoked(primary_account_id);
  EXPECT_TRUE(observer()->CheckEvents());

  SetActiveAccount(kPrimaryAccountEmail);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, PrimaryRevokeThenLogin) {
  std::string primary_account_id = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  NotifyLogoutOfAllAccounts();
  observer()->Clear();

  SetActiveAccount(kPrimaryAccountEmail);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, PrimaryLogoutThenRevoke) {
  std::string primary_account_id = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  observer()->Clear();

  NotifyLogoutOfAllAccounts();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, primary_account_id)));

  NotifyTokenRevoked(primary_account_id);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, PrimaryLogoutFetchCancelAvailable) {
  std::string primary_account_id = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  // TokenAvailable kicks off a fetch. Logout without satisfying it.
  NotifyLogoutOfAllAccounts();
  EXPECT_TRUE(observer()->CheckEvents());

  SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, primary_account_id)));
}
#endif

// Non-primary accounts

TEST_F(AccountTrackerTest, Available) {
  SetupPrimaryLogin();

  std::string account_id = AddAccountWithToken("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents());

  ReturnOAuthUrlFetchSuccess(account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account_id)));
}

TEST_F(AccountTrackerTest, AvailableRevokeAvailable) {
  SetupPrimaryLogin();

  std::string account_id = AddAccountWithToken("user@example.com");
  ReturnOAuthUrlFetchSuccess(account_id);
  NotifyTokenRevoked(account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account_id),
                                      TrackingEvent(SIGN_OUT, account_id)));

  NotifyTokenAvailable(account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account_id)));
}

TEST_F(AccountTrackerTest, AvailableRevokeAvailableWithPendingFetch) {
  SetupPrimaryLogin();

  std::string account_id = AddAccountWithToken("user@example.com");
  NotifyTokenRevoked(account_id);
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyTokenAvailable(account_id);
  ReturnOAuthUrlFetchSuccess(account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account_id)));
}

TEST_F(AccountTrackerTest, AvailableRevokeRevoke) {
  SetupPrimaryLogin();

  std::string account_id = AddAccountWithToken("user@example.com");
  ReturnOAuthUrlFetchSuccess(account_id);
  NotifyTokenRevoked(account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account_id),
                                      TrackingEvent(SIGN_OUT, account_id)));

  NotifyTokenRevoked(account_id);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, AvailableAvailable) {
  SetupPrimaryLogin();

  std::string account_id = AddAccountWithToken("user@example.com");
  ReturnOAuthUrlFetchSuccess(account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account_id)));

  NotifyTokenAvailable(account_id);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, TwoAccounts) {
  SetupPrimaryLogin();

  std::string alpha_account_id = AddAccountWithToken("alpha@example.com");
  ReturnOAuthUrlFetchSuccess(alpha_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, alpha_account_id)));

  std::string beta_account_id = AddAccountWithToken("beta@example.com");
  ReturnOAuthUrlFetchSuccess(beta_account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, beta_account_id)));

  NotifyTokenRevoked(alpha_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, alpha_account_id)));

  NotifyTokenRevoked(beta_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, beta_account_id)));
}

TEST_F(AccountTrackerTest, AvailableTokenFetchFailAvailable) {
  SetupPrimaryLogin();

  std::string account_id = AddAccountWithToken("user@example.com");
  ReturnOAuthUrlFetchFailure(account_id);
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyTokenAvailable(account_id);
  ReturnOAuthUrlFetchSuccess(account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account_id)));
}

// These tests exercise true login/logout, which are not possible on ChromeOS.
#if !defined(OS_CHROMEOS)
TEST_F(AccountTrackerTest, MultiSignOutSignIn) {
  std::string primary_account_id = SetupPrimaryLogin();

  std::string alpha_account_id = AddAccountWithToken("alpha@example.com");
  ReturnOAuthUrlFetchSuccess(alpha_account_id);
  std::string beta_account_id = AddAccountWithToken("beta@example.com");
  ReturnOAuthUrlFetchSuccess(beta_account_id);

  observer()->SortEventsByUser();
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, alpha_account_id),
                                      TrackingEvent(SIGN_IN, beta_account_id)));

  // Log out of the primary account only (allows for testing that the account
  // tracker preserves knowledge of "beta@example.com").
  NotifyLogoutOfPrimaryAccountOnly();
  observer()->SortEventsByUser();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, alpha_account_id),
                              TrackingEvent(SIGN_OUT, beta_account_id),
                              TrackingEvent(SIGN_OUT, primary_account_id)));

  // No events fire at all while profile is signed out.
  NotifyTokenRevoked(alpha_account_id);
  std::string gamma_account_id = AddAccountWithToken("gamma@example.com");
  EXPECT_TRUE(observer()->CheckEvents());

  // Signing the profile in again will resume tracking all accounts.
  SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(beta_account_id);
  ReturnOAuthUrlFetchSuccess(gamma_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  observer()->SortEventsByUser();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, beta_account_id),
                              TrackingEvent(SIGN_IN, gamma_account_id),
                              TrackingEvent(SIGN_IN, primary_account_id)));

  // Revoking the primary token does not affect other accounts.
  NotifyTokenRevoked(primary_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, primary_account_id)));

  NotifyTokenAvailable(primary_account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_IN, primary_account_id)));
}
#endif

// Primary/non-primary interactions

TEST_F(AccountTrackerTest, MultiNoEventsBeforeLogin) {
  std::string account_id1 = AddAccountWithToken("user@example.com");
  std::string account_id2 = AddAccountWithToken("user2@example.com");
  NotifyTokenRevoked(account_id2);
  NotifyTokenRevoked(account_id1);

// Logout is not possible on ChromeOS.
#if !defined(OS_CHROMEOS)
  NotifyLogoutOfAllAccounts();
#endif

  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, MultiRevokePrimaryDoesNotRemoveAllAccounts) {
  std::string primary_account_id = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  std::string account_id = AddAccountWithToken("user@example.com");
  ReturnOAuthUrlFetchSuccess(account_id);
  observer()->Clear();

  NotifyTokenRevoked(primary_account_id);
  observer()->SortEventsByUser();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, primary_account_id)));
}

TEST_F(AccountTrackerTest, GetAccountsPrimary) {
  std::string primary_account_id = SetupPrimaryLogin();

  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(1ul, ids.size());
  EXPECT_EQ(primary_account_id, ids[0].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(primary_account_id), ids[0].gaia);
}

TEST_F(AccountTrackerTest, GetAccountsSignedOut) {
  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(0ul, ids.size());
}

TEST_F(AccountTrackerTest, GetAccountsOnlyReturnAccountsWithTokens) {
  std::string primary_account_id = SetupPrimaryLogin();

  std::string alpha_account_id = AddAccountWithToken("alpha@example.com");
  std::string beta_account_id = AddAccountWithToken("beta@example.com");
  ReturnOAuthUrlFetchSuccess(beta_account_id);

  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(2ul, ids.size());
  EXPECT_EQ(primary_account_id, ids[0].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(primary_account_id), ids[0].gaia);
  EXPECT_EQ(beta_account_id, ids[1].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(beta_account_id), ids[1].gaia);
}

TEST_F(AccountTrackerTest, GetAccountsSortOrder) {
  std::string primary_account_id = SetupPrimaryLogin();

  std::string zeta_account_id = AddAccountWithToken("zeta@example.com");
  ReturnOAuthUrlFetchSuccess(zeta_account_id);
  std::string alpha_account_id = AddAccountWithToken("alpha@example.com");
  ReturnOAuthUrlFetchSuccess(alpha_account_id);

  // The primary account will be first in the vector. Remaining accounts
  // will be sorted by gaia ID.
  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(3ul, ids.size());
  EXPECT_EQ(primary_account_id, ids[0].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(primary_account_id), ids[0].gaia);
  EXPECT_EQ(alpha_account_id, ids[1].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(alpha_account_id), ids[1].gaia);
  EXPECT_EQ(zeta_account_id, ids[2].account_key);
  EXPECT_EQ(AccountKeyToObfuscatedId(zeta_account_id), ids[2].gaia);
}

TEST_F(AccountTrackerTest, GetAccountsReturnNothingWhenPrimarySignedOut) {
  std::string primary_account_id = SetupPrimaryLogin();

  std::string zeta_account_id = AddAccountWithToken("zeta@example.com");
  ReturnOAuthUrlFetchSuccess(zeta_account_id);
  std::string alpha_account_id = AddAccountWithToken("alpha@example.com");
  ReturnOAuthUrlFetchSuccess(alpha_account_id);

  NotifyTokenRevoked(primary_account_id);

  std::vector<AccountIds> ids = account_tracker()->GetAccounts();
  EXPECT_EQ(0ul, ids.size());
}

// This test exercises true login/logout, which are not possible on ChromeOS.
#if !defined(OS_CHROMEOS)
TEST_F(AccountTrackerTest, MultiLogoutRemovesAllAccounts) {
  std::string primary_account_id = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account_id);
  ReturnOAuthUrlFetchSuccess(primary_account_id);
  std::string account_id = AddAccountWithToken("user@example.com");
  ReturnOAuthUrlFetchSuccess(account_id);
  observer()->Clear();

  NotifyLogoutOfAllAccounts();
  observer()->SortEventsByUser();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, primary_account_id),
                              TrackingEvent(SIGN_OUT, account_id)));
}
#endif

}  // namespace gcm
