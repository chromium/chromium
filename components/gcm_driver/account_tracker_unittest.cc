// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/account_tracker.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPrimaryAccountEmail[] = "primary_account@example.com";

enum TrackingEventType { SIGN_IN, SIGN_OUT };

class TrackingEvent {
 public:
  TrackingEvent(TrackingEventType type,
                const CoreAccountId& account_id,
                const std::string& gaia_id)
      : type_(type), account_id_(account_id), gaia_id_(gaia_id) {}

  TrackingEvent(TrackingEventType type, const CoreAccountInfo& account_info)
      : type_(type),
        account_id_(account_info.account_id),
        gaia_id_(account_info.gaia) {}

  bool operator==(const TrackingEvent& event) const {
    return type_ == event.type_ && account_id_ == event.account_id_ &&
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
    return base::StringPrintf("{ type: %s, account_id: %s, gaia: %s }", typestr,
                              account_id_.ToString().c_str(), gaia_id_.c_str());
  }

 private:
  friend bool CompareByUser(TrackingEvent a, TrackingEvent b);

  TrackingEventType type_;
  CoreAccountId account_id_;
  std::string gaia_id_;
};

bool CompareByUser(TrackingEvent a, TrackingEvent b) {
  return a.account_id_ < b.account_id_;
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
  AccountTrackerObserver() = default;
  virtual ~AccountTrackerObserver() = default;

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
  void OnAccountSignInChanged(const CoreAccountInfo& account,
                              bool is_signed_in) override;

 private:
  testing::AssertionResult CheckEvents(
      const std::vector<TrackingEvent>& events);

  std::vector<TrackingEvent> events_;
};

void AccountTrackerObserver::OnAccountSignInChanged(
    const CoreAccountInfo& account,
    bool is_signed_in) {
  events_.push_back(TrackingEvent(is_signed_in ? SIGN_IN : SIGN_OUT,
                                  account.account_id, account.gaia));
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
  AccountTrackerTest() = default;

  ~AccountTrackerTest() override = default;

  void SetUp() override {
    account_tracker_ =
        std::make_unique<AccountTracker>(identity_test_env_.identity_manager());
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
  CoreAccountInfo SetActiveAccount(const std::string& email) {
    // TODO(crbug.com/40067875): Delete account-tracking code, latest when
    // ConsentLevel::kSync is cleaned up from the codebase.
    return identity_test_env_.SetPrimaryAccount(email,
                                                signin::ConsentLevel::kSync);
  }

// Helpers that go through a logout flow.
// NOTE: On ChromeOS, the logout callback is never fired in production (since
// the underlying GoogleSignedOut callback is never sent). Tests that exercise
// functionality dependent on that callback firing are not relevant on ChromeOS
// and should simply not run on that platform.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void NotifyLogoutOfAllAccounts() { identity_test_env_.ClearPrimaryAccount(); }
#endif

  CoreAccountInfo AddAccountWithToken(const std::string& email) {
    return identity_test_env_.MakeAccountAvailable(email);
  }

  void NotifyTokenAvailable(const CoreAccountId& account_id) {
    identity_test_env_.SetRefreshTokenForAccount(account_id);
  }

  void NotifyTokenRevoked(const CoreAccountId& account_id) {
    identity_test_env_.RemoveRefreshTokenForAccount(account_id);
  }

  CoreAccountInfo SetupPrimaryLogin() {
    // Initial setup for tests that start with a signed in profile.
    CoreAccountInfo primary_account = SetActiveAccount(kPrimaryAccountEmail);
    NotifyTokenAvailable(primary_account.account_id);
    observer()->Clear();

    return primary_account;
  }

 private:
  // net:: stuff needs IO message loop.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  signin::IdentityTestEnvironment identity_test_env_;

  std::unique_ptr<AccountTracker> account_tracker_;
  AccountTrackerObserver observer_;
};

// Primary tests just involve the Active account

TEST_F(AccountTrackerTest, PrimaryNoEventsBeforeLogin) {
  CoreAccountInfo account = AddAccountWithToken("me@dummy.com");
  NotifyTokenRevoked(account.account_id);

// Logout is not possible on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  NotifyLogoutOfAllAccounts();
#endif

  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, PrimaryLoginThenTokenAvailable) {
  CoreAccountInfo primary_account = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account.account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, primary_account)));
}

TEST_F(AccountTrackerTest, PrimaryRevoke) {
  CoreAccountInfo primary_account = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account.account_id);
  observer()->Clear();

  NotifyTokenRevoked(primary_account.account_id);
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, primary_account)));
}

TEST_F(AccountTrackerTest, PrimaryRevokeThenTokenAvailable) {
  CoreAccountInfo primary_account = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account.account_id);
  NotifyTokenRevoked(primary_account.account_id);
  observer()->Clear();

  NotifyTokenAvailable(primary_account.account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, primary_account)));
}

// These tests exercise true login/logout, which are not possible on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(AccountTrackerTest, PrimaryTokenAvailableThenLogin) {
  AddAccountWithToken(kPrimaryAccountEmail);
  EXPECT_TRUE(observer()->CheckEvents());

  CoreAccountInfo primary_account = SetActiveAccount(kPrimaryAccountEmail);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, primary_account)));
}

TEST_F(AccountTrackerTest, PrimaryTokenAvailableAndRevokedThenLogin) {
  CoreAccountInfo primary_account = AddAccountWithToken(kPrimaryAccountEmail);
  EXPECT_TRUE(observer()->CheckEvents());

  NotifyTokenRevoked(primary_account.account_id);
  EXPECT_TRUE(observer()->CheckEvents());

  SetActiveAccount(kPrimaryAccountEmail);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, PrimaryRevokeThenLogin) {
  CoreAccountInfo primary_account = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account.account_id);
  NotifyLogoutOfAllAccounts();
  observer()->Clear();

  SetActiveAccount(kPrimaryAccountEmail);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, PrimaryLogoutThenRevoke) {
  CoreAccountInfo primary_account = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account.account_id);
  observer()->Clear();

  NotifyLogoutOfAllAccounts();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, primary_account)));

  NotifyTokenRevoked(primary_account.account_id);
  EXPECT_TRUE(observer()->CheckEvents());
}

#endif

// Non-primary accounts

TEST_F(AccountTrackerTest, Available) {
  SetupPrimaryLogin();

  CoreAccountInfo account = AddAccountWithToken("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account)));
}

TEST_F(AccountTrackerTest, AvailableRevokeAvailable) {
  SetupPrimaryLogin();

  CoreAccountInfo account = AddAccountWithToken("user@example.com");
  NotifyTokenRevoked(account.account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account),
                                      TrackingEvent(SIGN_OUT, account)));

  NotifyTokenAvailable(account.account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account)));
}

TEST_F(AccountTrackerTest, AvailableRevokeRevoke) {
  SetupPrimaryLogin();

  CoreAccountInfo account = AddAccountWithToken("user@example.com");
  NotifyTokenRevoked(account.account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account),
                                      TrackingEvent(SIGN_OUT, account)));

  NotifyTokenRevoked(account.account_id);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, AvailableAvailable) {
  SetupPrimaryLogin();

  CoreAccountInfo account = AddAccountWithToken("user@example.com");
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, account)));

  NotifyTokenAvailable(account.account_id);
  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, TwoAccounts) {
  SetupPrimaryLogin();

  CoreAccountInfo alpha_account = AddAccountWithToken("alpha@example.com");
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, alpha_account)));

  CoreAccountInfo beta_account = AddAccountWithToken("beta@example.com");
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_IN, beta_account)));

  NotifyTokenRevoked(alpha_account.account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_OUT, alpha_account)));

  NotifyTokenRevoked(beta_account.account_id);
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_OUT, beta_account)));
}

// Primary/non-primary interactions

TEST_F(AccountTrackerTest, MultiNoEventsBeforeLogin) {
  CoreAccountInfo account1 = AddAccountWithToken("user@example.com");
  CoreAccountInfo account2 = AddAccountWithToken("user2@example.com");
  NotifyTokenRevoked(account2.account_id);
  NotifyTokenRevoked(account2.account_id);

// Logout is not possible on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  NotifyLogoutOfAllAccounts();
#endif

  EXPECT_TRUE(observer()->CheckEvents());
}

TEST_F(AccountTrackerTest, MultiRevokePrimaryDoesNotRemoveAllAccounts) {
  CoreAccountInfo primary_account = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account.account_id);
  CoreAccountInfo account = AddAccountWithToken("user@example.com");
  observer()->Clear();

  NotifyTokenRevoked(primary_account.account_id);
  observer()->SortEventsByUser();
  EXPECT_TRUE(
      observer()->CheckEvents(TrackingEvent(SIGN_OUT, primary_account)));
}

TEST_F(AccountTrackerTest, GetAccountsPrimary) {
  CoreAccountInfo primary_account = SetupPrimaryLogin();

  std::vector<CoreAccountInfo> account = account_tracker()->GetAccounts();
  EXPECT_EQ(1ul, account.size());
  EXPECT_EQ(primary_account.account_id, account[0].account_id);
  EXPECT_EQ(primary_account.gaia, account[0].gaia);
  EXPECT_EQ(primary_account.email, account[0].email);
}

TEST_F(AccountTrackerTest, GetAccountsSignedOut) {
  std::vector<CoreAccountInfo> accounts = account_tracker()->GetAccounts();
  EXPECT_EQ(0ul, accounts.size());
}

TEST_F(AccountTrackerTest, GetMultipleAccounts) {
  CoreAccountInfo primary_account = SetupPrimaryLogin();
  CoreAccountInfo alpha_account = AddAccountWithToken("alpha@example.com");
  CoreAccountInfo beta_account = AddAccountWithToken("beta@example.com");

  std::vector<CoreAccountInfo> account = account_tracker()->GetAccounts();
  EXPECT_EQ(3ul, account.size());
  EXPECT_EQ(primary_account.account_id, account[0].account_id);
  EXPECT_EQ(primary_account.email, account[0].email);
  EXPECT_EQ(primary_account.gaia, account[0].gaia);

  EXPECT_EQ(alpha_account.account_id, account[1].account_id);
  EXPECT_EQ(alpha_account.email, account[1].email);
  EXPECT_EQ(alpha_account.gaia, account[1].gaia);

  EXPECT_EQ(beta_account.account_id, account[2].account_id);
  EXPECT_EQ(beta_account.email, account[2].email);
  EXPECT_EQ(beta_account.gaia, account[2].gaia);
}

TEST_F(AccountTrackerTest, GetAccountsReturnNothingWhenPrimarySignedOut) {
  CoreAccountInfo primary_account = SetupPrimaryLogin();

  CoreAccountInfo zeta_account = AddAccountWithToken("zeta@example.com");
  CoreAccountInfo alpha_account = AddAccountWithToken("alpha@example.com");

  NotifyTokenRevoked(primary_account.account_id);

  std::vector<CoreAccountInfo> account = account_tracker()->GetAccounts();
  EXPECT_EQ(0ul, account.size());
}

// This test exercises true login/logout, which are not possible on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(AccountTrackerTest, MultiLogoutRemovesAllAccounts) {
  CoreAccountInfo primary_account = SetActiveAccount(kPrimaryAccountEmail);
  NotifyTokenAvailable(primary_account.account_id);
  CoreAccountInfo account = AddAccountWithToken("user@example.com");
  observer()->Clear();

  NotifyLogoutOfAllAccounts();
  observer()->SortEventsByUser();
  EXPECT_TRUE(observer()->CheckEvents(TrackingEvent(SIGN_OUT, primary_account),
                                      TrackingEvent(SIGN_OUT, account)));
}
#endif

}  // namespace gcm
