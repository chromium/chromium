// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_account_tracker.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const char kEmail1[] = "account_1@me.com";
const char kEmail2[] = "account_2@me.com";

std::string MakeAccessToken(const CoreAccountId& account_id) {
  return "access_token-" + account_id.ToString();
}

GCMClient::AccountTokenInfo MakeAccountToken(const CoreAccountInfo& account) {
  GCMClient::AccountTokenInfo token_info;
  token_info.account_id = account.account_id;

  // TODO(https://crbug.com/856170): This *should* be expected to be the email
  // address for the given account, but there is a bug in AccountTracker that
  // means that |token_info.email| actually gets populated with the account ID
  // by the production code. Hence the test expectation has to match what the
  // production code actually does :). If/when that bug gets fixed, this
  // function should be changed to take in the email address as well as the
  // account ID and populate this field with the email address.
  token_info.email = account.email;
  token_info.access_token = MakeAccessToken(account.account_id);
  return token_info;
}

void VerifyAccountTokens(
    const std::vector<GCMClient::AccountTokenInfo>& expected_tokens,
    const std::vector<GCMClient::AccountTokenInfo>& actual_tokens) {
  EXPECT_EQ(expected_tokens.size(), actual_tokens.size());
  for (auto expected_iter = expected_tokens.begin(),
            actual_iter = actual_tokens.begin();
       expected_iter != expected_tokens.end() &&
       actual_iter != actual_tokens.end();
       ++expected_iter, ++actual_iter) {
    EXPECT_EQ(expected_iter->account_id, actual_iter->account_id);
    EXPECT_EQ(expected_iter->email, actual_iter->email);
    EXPECT_EQ(expected_iter->access_token, actual_iter->access_token);
  }
}

// This version of FakeGCMDriver is customized around handling accounts and
// connection events for testing GCMAccountTracker.
class CustomFakeGCMDriver : public FakeGCMDriver {
 public:
  CustomFakeGCMDriver();
  ~CustomFakeGCMDriver() override;

  // GCMDriver overrides:
  void SetAccountTokens(
      const std::vector<GCMClient::AccountTokenInfo>& account_tokens) override;
  void AddConnectionObserver(GCMConnectionObserver* observer) override;
  void RemoveConnectionObserver(GCMConnectionObserver* observer) override;
  bool IsConnected() const override { return connected_; }
  base::Time GetLastTokenFetchTime() override;
  void SetLastTokenFetchTime(const base::Time& time) override;

  // Test results and helpers.
  void SetConnected(bool connected);
  void ResetResults();
  bool update_accounts_called() const { return update_accounts_called_; }
  const std::vector<GCMClient::AccountTokenInfo>& accounts() const {
    return accounts_;
  }
  const GCMConnectionObserver* last_connection_observer() const {
    return last_connection_observer_;
  }
  const GCMConnectionObserver* last_removed_connection_observer() const {
    return removed_connection_observer_;
  }

 private:
  bool connected_;
  std::vector<GCMClient::AccountTokenInfo> accounts_;
  bool update_accounts_called_;
  GCMConnectionObserver* last_connection_observer_;
  GCMConnectionObserver* removed_connection_observer_;
  net::IPEndPoint ip_endpoint_;
  base::Time last_token_fetch_time_;

  DISALLOW_COPY_AND_ASSIGN(CustomFakeGCMDriver);
};

CustomFakeGCMDriver::CustomFakeGCMDriver()
    : connected_(true),
      update_accounts_called_(false),
      last_connection_observer_(nullptr),
      removed_connection_observer_(nullptr) {}

CustomFakeGCMDriver::~CustomFakeGCMDriver() {
}

void CustomFakeGCMDriver::SetAccountTokens(
    const std::vector<GCMClient::AccountTokenInfo>& accounts) {
  update_accounts_called_ = true;
  accounts_ = accounts;
}

void CustomFakeGCMDriver::AddConnectionObserver(
    GCMConnectionObserver* observer) {
  last_connection_observer_ = observer;
}

void CustomFakeGCMDriver::RemoveConnectionObserver(
    GCMConnectionObserver* observer) {
  removed_connection_observer_ = observer;
}

void CustomFakeGCMDriver::SetConnected(bool connected) {
  connected_ = connected;
  if (connected && last_connection_observer_)
    last_connection_observer_->OnConnected(ip_endpoint_);
}

void CustomFakeGCMDriver::ResetResults() {
  accounts_.clear();
  update_accounts_called_ = false;
  last_connection_observer_ = nullptr;
  removed_connection_observer_ = nullptr;
}


base::Time CustomFakeGCMDriver::GetLastTokenFetchTime() {
  return last_token_fetch_time_;
}

void CustomFakeGCMDriver::SetLastTokenFetchTime(const base::Time& time) {
  last_token_fetch_time_ = time;
}

}  // namespace

class GCMAccountTrackerTest : public testing::Test {
 public:
  GCMAccountTrackerTest();
  ~GCMAccountTrackerTest() override;

  // Helpers to pass fake info to the tracker.
  CoreAccountInfo AddAccount(const std::string& email);
  CoreAccountInfo SetPrimaryAccount(const std::string& email);
  void RemoveAccount(const CoreAccountId& account_id);

  // Helpers for dealing with OAuth2 access token requests.
  void IssueAccessToken(const CoreAccountId& account_id);
  void IssueExpiredAccessToken(const CoreAccountId& account_id);
  void IssueError(const CoreAccountId& account_id);

  // Accessors to account tracker and gcm driver.
  GCMAccountTracker* tracker() { return tracker_.get(); }
  CustomFakeGCMDriver* driver() { return &driver_; }

  // Accessors to private methods of account tracker.
  bool IsFetchingRequired() const;
  bool IsTokenReportingRequired() const;
  base::TimeDelta GetTimeToNextTokenReporting() const;

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  CustomFakeGCMDriver driver_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;

  std::unique_ptr<GCMAccountTracker> tracker_;
};

GCMAccountTrackerTest::GCMAccountTrackerTest() {
  std::unique_ptr<AccountTracker> gaia_account_tracker(
      new AccountTracker(identity_test_env_.identity_manager()));

  tracker_.reset(new GCMAccountTracker(std::move(gaia_account_tracker),
                                       identity_test_env_.identity_manager(),
                                       &driver_));
}

GCMAccountTrackerTest::~GCMAccountTrackerTest() {
  if (tracker_)
    tracker_->Shutdown();
}

CoreAccountInfo GCMAccountTrackerTest::AddAccount(const std::string& email) {
  return identity_test_env_.MakeAccountAvailable(email);
}

CoreAccountInfo GCMAccountTrackerTest::SetPrimaryAccount(
    const std::string& email) {
  // NOTE: Setting of the primary account info must be done first on ChromeOS
  // to ensure that AccountTracker and GCMAccountTracker respond as expected
  // when the token is added to the token service.
  // TODO(blundell): On non-ChromeOS, it would be good to add tests wherein
  // setting of the primary account is done afterward to check that the flow
  // that ensues from the GoogleSigninSucceeded callback firing works as
  // expected.
  return identity_test_env_.MakePrimaryAccountAvailable(email);
}

void GCMAccountTrackerTest::RemoveAccount(const CoreAccountId& account_id) {
  identity_test_env_.RemoveRefreshTokenForAccount(account_id);
}

void GCMAccountTrackerTest::IssueAccessToken(const CoreAccountId& account_id) {
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      account_id, MakeAccessToken(account_id), base::Time::Max());
}

void GCMAccountTrackerTest::IssueExpiredAccessToken(
    const CoreAccountId& account_id) {
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      account_id, MakeAccessToken(account_id), base::Time::Now());
}

void GCMAccountTrackerTest::IssueError(const CoreAccountId& account_id) {
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
}

bool GCMAccountTrackerTest::IsFetchingRequired() const {
  return tracker_->IsTokenFetchingRequired();
}

bool GCMAccountTrackerTest::IsTokenReportingRequired() const {
  return tracker_->IsTokenReportingRequired();
}

base::TimeDelta GCMAccountTrackerTest::GetTimeToNextTokenReporting() const {
  return tracker_->GetTimeToNextTokenReporting();
}

TEST_F(GCMAccountTrackerTest, NoAccounts) {
  EXPECT_FALSE(driver()->update_accounts_called());
  tracker()->Start();
  // Callback should not be called if there where no accounts provided.
  EXPECT_FALSE(driver()->update_accounts_called());
  EXPECT_TRUE(driver()->accounts().empty());
}

// Verifies that callback is called after a token is issued for a single account
// with a specific scope. In this scenario, the underlying account tracker is
// still working when the CompleteCollectingTokens is called for the first time.
TEST_F(GCMAccountTrackerTest, SingleAccount) {
  CoreAccountInfo account1 = SetPrimaryAccount(kEmail1);

  tracker()->Start();
  EXPECT_FALSE(driver()->update_accounts_called());

  IssueAccessToken(account1.account_id);
  EXPECT_TRUE(driver()->update_accounts_called());

  std::vector<GCMClient::AccountTokenInfo> expected_accounts;
  expected_accounts.push_back(MakeAccountToken(account1));
  VerifyAccountTokens(expected_accounts, driver()->accounts());
}

TEST_F(GCMAccountTrackerTest, MultipleAccounts) {
  CoreAccountInfo account1 = SetPrimaryAccount(kEmail1);
  CoreAccountInfo account2 = AddAccount(kEmail2);

  tracker()->Start();
  EXPECT_FALSE(driver()->update_accounts_called());

  IssueAccessToken(account1.account_id);
  EXPECT_FALSE(driver()->update_accounts_called());

  IssueAccessToken(account2.account_id);
  EXPECT_TRUE(driver()->update_accounts_called());

  std::vector<GCMClient::AccountTokenInfo> expected_accounts;
  expected_accounts.push_back(MakeAccountToken(account1));
  expected_accounts.push_back(MakeAccountToken(account2));
  VerifyAccountTokens(expected_accounts, driver()->accounts());
}

TEST_F(GCMAccountTrackerTest, AccountAdded) {
  tracker()->Start();
  driver()->ResetResults();

  CoreAccountInfo account1 = SetPrimaryAccount(kEmail1);
  EXPECT_FALSE(driver()->update_accounts_called());

  IssueAccessToken(account1.account_id);
  EXPECT_TRUE(driver()->update_accounts_called());

  std::vector<GCMClient::AccountTokenInfo> expected_accounts;
  expected_accounts.push_back(MakeAccountToken(account1));
  VerifyAccountTokens(expected_accounts, driver()->accounts());
}

TEST_F(GCMAccountTrackerTest, AccountRemoved) {
  CoreAccountInfo account1 = SetPrimaryAccount(kEmail1);
  CoreAccountInfo account2 = AddAccount(kEmail2);

  tracker()->Start();
  IssueAccessToken(account1.account_id);
  IssueAccessToken(account2.account_id);
  EXPECT_TRUE(driver()->update_accounts_called());

  driver()->ResetResults();
  EXPECT_FALSE(driver()->update_accounts_called());

  RemoveAccount(account2.account_id);
  EXPECT_TRUE(driver()->update_accounts_called());

  std::vector<GCMClient::AccountTokenInfo> expected_accounts;
  expected_accounts.push_back(MakeAccountToken(account1));
  VerifyAccountTokens(expected_accounts, driver()->accounts());
}

TEST_F(GCMAccountTrackerTest, GetTokenFailed) {
  CoreAccountInfo account1 = SetPrimaryAccount(kEmail1);
  CoreAccountInfo account2 = AddAccount(kEmail2);

  tracker()->Start();
  IssueAccessToken(account1.account_id);
  EXPECT_FALSE(driver()->update_accounts_called());

  IssueError(account2.account_id);

  // Failed token is not retried any more. Account marked as removed.
  EXPECT_EQ(0UL, tracker()->get_pending_token_request_count());
  EXPECT_TRUE(driver()->update_accounts_called());

  std::vector<GCMClient::AccountTokenInfo> expected_accounts;
  expected_accounts.push_back(MakeAccountToken(account1));
  VerifyAccountTokens(expected_accounts, driver()->accounts());
}

TEST_F(GCMAccountTrackerTest, GetTokenFailedAccountRemoved) {
  CoreAccountInfo account1 = SetPrimaryAccount(kEmail1);
  CoreAccountInfo account2 = AddAccount(kEmail2);

  tracker()->Start();
  IssueAccessToken(account1.account_id);

  driver()->ResetResults();
  RemoveAccount(account2.account_id);
  IssueError(account2.account_id);

  EXPECT_TRUE(driver()->update_accounts_called());

  std::vector<GCMClient::AccountTokenInfo> expected_accounts;
  expected_accounts.push_back(MakeAccountToken(account1));
  VerifyAccountTokens(expected_accounts, driver()->accounts());
}

TEST_F(GCMAccountTrackerTest, AccountRemovedWhileRequestsPending) {
  CoreAccountInfo account1 = SetPrimaryAccount(kEmail1);
  CoreAccountInfo account2 = AddAccount(kEmail2);

  tracker()->Start();
  IssueAccessToken(account1.account_id);
  EXPECT_FALSE(driver()->update_accounts_called());

  RemoveAccount(account2.account_id);
  IssueAccessToken(account2.account_id);
  EXPECT_TRUE(driver()->update_accounts_called());

  std::vector<GCMClient::AccountTokenInfo> expected_accounts;
  expected_accounts.push_back(MakeAccountToken(account1));
  VerifyAccountTokens(expected_accounts, driver()->accounts());
}

// Makes sure that tracker observes GCM connection when running.
TEST_F(GCMAccountTrackerTest, TrackerObservesConnection) {
  EXPECT_EQ(nullptr, driver()->last_connection_observer());
  tracker()->Start();
  EXPECT_EQ(tracker(), driver()->last_connection_observer());
  tracker()->Shutdown();
  EXPECT_EQ(tracker(), driver()->last_removed_connection_observer());
}

// Makes sure that token fetching happens only after connection is established.
TEST_F(GCMAccountTrackerTest, PostponeTokenFetchingUntilConnected) {
  driver()->SetConnected(false);
  CoreAccountInfo account1 = SetPrimaryAccount(kEmail1);
  tracker()->Start();

  EXPECT_EQ(0UL, tracker()->get_pending_token_request_count());
  driver()->SetConnected(true);

  EXPECT_EQ(1UL, tracker()->get_pending_token_request_count());
}

TEST_F(GCMAccountTrackerTest, InvalidateExpiredTokens) {
  CoreAccountInfo account1 = SetPrimaryAccount(kEmail1);
  CoreAccountInfo account2 = AddAccount(kEmail2);
  tracker()->Start();

  EXPECT_EQ(2UL, tracker()->get_pending_token_request_count());

  IssueExpiredAccessToken(account1.account_id);
  IssueAccessToken(account2.account_id);
  // Because the first token is expired, we expect the sanitize to kick in and
  // clean it up before the SetAccessToken is called. This also means a new
  // token request will be issued
  EXPECT_FALSE(driver()->update_accounts_called());
  EXPECT_EQ(1UL, tracker()->get_pending_token_request_count());
}

// Testing for whether there are still more tokens to be fetched. Typically the
// need for token fetching triggers immediate request, unless there is no
// connection, that is why connection is set on and off in this test.
TEST_F(GCMAccountTrackerTest, IsTokenFetchingRequired) {
  tracker()->Start();
  driver()->SetConnected(false);
  EXPECT_FALSE(IsFetchingRequired());
  CoreAccountInfo account1 = SetPrimaryAccount(kEmail1);
  EXPECT_TRUE(IsFetchingRequired());

  driver()->SetConnected(true);
  EXPECT_FALSE(IsFetchingRequired());  // Indicates that fetching has started.
  IssueAccessToken(account1.account_id);
  EXPECT_FALSE(IsFetchingRequired());

  CoreAccountInfo account2 = AddAccount(kEmail2);
  EXPECT_FALSE(IsFetchingRequired());  // Indicates that fetching has started.

  // Disconnect the driver again so that the access token request being
  // fulfilled doesn't immediately cause another access token request (which
  // then would cause IsFetchingRequired() to be false, preventing us from
  // distinguishing this case from the case where IsFetchingRequired() is false
  // because GCMAccountTracker didn't detect that a new access token needs to be
  // fetched).
  driver()->SetConnected(false);
  IssueExpiredAccessToken(account2.account_id);

  // Make sure that if the token was expired it is marked as being needed again.
  EXPECT_TRUE(IsFetchingRequired());
}

// Tests what is the expected time to the next token fetching.
TEST_F(GCMAccountTrackerTest, GetTimeToNextTokenReporting) {
  tracker()->Start();
  // At this point the last token fetch time is never.
  EXPECT_EQ(base::TimeDelta(), GetTimeToNextTokenReporting());

  // Regular case. The tokens have been just reported.
  driver()->SetLastTokenFetchTime(base::Time::Now());
  EXPECT_TRUE(GetTimeToNextTokenReporting() <=
                  base::TimeDelta::FromSeconds(12 * 60 * 60));

  // A case when gcm driver is not yet initialized.
  driver()->SetLastTokenFetchTime(base::Time::Max());
  EXPECT_EQ(base::TimeDelta::FromSeconds(12 * 60 * 60),
            GetTimeToNextTokenReporting());

  // A case when token reporting calculation is expected to result in more than
  // 12 hours, in which case we expect exactly 12 hours.
  driver()->SetLastTokenFetchTime(base::Time::Now() +
      base::TimeDelta::FromDays(2));
  EXPECT_EQ(base::TimeDelta::FromSeconds(12 * 60 * 60),
            GetTimeToNextTokenReporting());
}

// Tests conditions when token reporting is required.
TEST_F(GCMAccountTrackerTest, IsTokenReportingRequired) {
  tracker()->Start();
  // Required because it is overdue.
  EXPECT_TRUE(IsTokenReportingRequired());

  // Not required because it just happened.
  driver()->SetLastTokenFetchTime(base::Time::Now());
  EXPECT_FALSE(IsTokenReportingRequired());

  CoreAccountInfo account1 = SetPrimaryAccount(kEmail1);
  IssueAccessToken(account1.account_id);
  driver()->ResetResults();
  // Reporting was triggered, which means testing for required will give false,
  // but we have the update call.
  RemoveAccount(account1.account_id);
  EXPECT_TRUE(driver()->update_accounts_called());
  EXPECT_FALSE(IsTokenReportingRequired());
}

// TODO(fgorski): Add test for adding account after removal >> make sure it does
// not mark removal.

}  // namespace gcm
