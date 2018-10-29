// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_account_tracker.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_status_code.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const char kOAuthURL[] = "https://www.googleapis.com/oauth2/v1/userinfo";

const char kEmail1[] = "account_1@me.com";
const char kEmail2[] = "account_2@me.com";

std::string AccountIdToObfuscatedId(const std::string& account_id) {
  return "obfid-" + account_id;
}

std::string GetValidTokenInfoResponse(const std::string& account_id) {
  return std::string("{ \"id\": \"") + AccountIdToObfuscatedId(account_id) +
         "\" }";
}

std::string MakeAccessToken(const std::string& account_id) {
  return "access_token-" + account_id;
}

GCMClient::AccountTokenInfo MakeAccountToken(const std::string& account_id) {
  GCMClient::AccountTokenInfo token_info;
  token_info.account_id = account_id;

  // TODO(https://crbug.com/856170): This *should* be expected to be the email
  // address for the given account, but there is a bug in AccountTracker that
  // means that |token_info.email| actually gets populated with the account ID
  // by the production code. Hence the test expectation has to match what the
  // production code actually does :). If/when that bug gets fixed, this
  // function should be changed to take in the email address as well as the
  // account ID and populate this field with the email address.
  token_info.email = account_id;
  token_info.access_token = MakeAccessToken(account_id);
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

  // Helpers to pass fake info to the tracker. Tests should have either a pair
  // of Start(Primary)/FinishAccountAddition or Add(Primary)Account per
  // account. Don't mix. Any methods that return an std::string are returning
  // the account ID of the newly-added account, which can then be passed into
  // any methods that take in an account ID.
  // Call to RemoveAccount is not mandatory.
  std::string StartAccountAddition(const std::string& email);
  std::string StartPrimaryAccountAddition(const std::string& email);
  void FinishAccountAddition(const std::string& account_id);
  std::string AddAccount(const std::string& email);
  std::string AddPrimaryAccount(const std::string& email);
  void RemoveAccount(const std::string& account_id);

  // Helpers for dealing with OAuth2 access token requests.
  void IssueAccessToken(const std::string& account_id);
  void IssueExpiredAccessToken(const std::string& account_id);
  void IssueError(const std::string& account_id);

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

  base::MessageLoop message_loop_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  identity::IdentityTestEnvironment identity_test_env_;

  std::unique_ptr<GCMAccountTracker> tracker_;
};

GCMAccountTrackerTest::GCMAccountTrackerTest() {
  std::unique_ptr<AccountTracker> gaia_account_tracker(new AccountTracker(
      identity_test_env_.identity_manager(),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_)));

  tracker_.reset(new GCMAccountTracker(std::move(gaia_account_tracker),
                                       identity_test_env_.identity_manager(),
                                       &driver_));
}

GCMAccountTrackerTest::~GCMAccountTrackerTest() {
  if (tracker_)
    tracker_->Shutdown();
}

std::string GCMAccountTrackerTest::StartAccountAddition(
    const std::string& email) {
  return identity_test_env_.MakeAccountAvailable(email).account_id;
}

std::string GCMAccountTrackerTest::StartPrimaryAccountAddition(
    const std::string& email) {
// NOTE: Setting of the primary account info must be done first on ChromeOS
// to ensure that AccountTracker and GCMAccountTracker respond as expected
// when the token is added to the token service.
// TODO(blundell): On non-ChromeOS, it would be good to add tests wherein
// setting of the primary account is done afterward to check that the flow
// that ensues from the GoogleSigninSucceeded callback firing works as
// expected.
return identity_test_env_.MakePrimaryAccountAvailable(email).account_id;
}

void GCMAccountTrackerTest::FinishAccountAddition(
    const std::string& account_id) {
  IssueAccessToken(account_id);

  EXPECT_TRUE(test_url_loader_factory()->IsPending(kOAuthURL));
  test_url_loader_factory()->SimulateResponseForPendingRequest(
      GURL(kOAuthURL), network::URLLoaderCompletionStatus(net::OK),
      network::CreateResourceResponseHead(net::HTTP_OK),
      GetValidTokenInfoResponse(account_id));

  GetValidTokenInfoResponse(account_id);
}

std::string GCMAccountTrackerTest::AddPrimaryAccount(const std::string& email) {
  std::string account_id = StartPrimaryAccountAddition(email);
  FinishAccountAddition(account_id);
  return account_id;
}

std::string GCMAccountTrackerTest::AddAccount(const std::string& email) {
  std::string account_id = StartAccountAddition(email);
  FinishAccountAddition(account_id);
  return account_id;
}

void GCMAccountTrackerTest::RemoveAccount(const std::string& account_id) {
  identity_test_env_.RemoveRefreshTokenForAccount(account_id);
}

void GCMAccountTrackerTest::IssueAccessToken(const std::string& account_id) {
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      account_id, MakeAccessToken(account_id), base::Time::Max());
}

void GCMAccountTrackerTest::IssueExpiredAccessToken(
    const std::string& account_id) {
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      account_id, MakeAccessToken(account_id), base::Time::Now());
}

void GCMAccountTrackerTest::IssueError(const std::string& account_id) {
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
  std::string account_id1 = StartPrimaryAccountAddition(kEmail1);

  tracker()->Start();
  // We don't have any accounts to report, but given the inner account tracker
  // is still working we don't make a call with empty accounts list.
  EXPECT_FALSE(driver()->update_accounts_called());

  // This concludes the work of inner account tracker.
  FinishAccountAddition(account_id1);
  IssueAccessToken(account_id1);

  EXPECT_TRUE(driver()->update_accounts_called());

  std::vector<GCMClient::AccountTokenInfo> expected_accounts;
  expected_accounts.push_back(MakeAccountToken(account_id1));
  VerifyAccountTokens(expected_accounts, driver()->accounts());
}

TEST_F(GCMAccountTrackerTest, MultipleAccounts) {
  std::string account_id1 = StartPrimaryAccountAddition(kEmail1);

  std::string account_id2 = StartAccountAddition(kEmail2);

  tracker()->Start();
  EXPECT_FALSE(driver()->update_accounts_called());

  FinishAccountAddition(account_id1);
  IssueAccessToken(account_id1);
  EXPECT_FALSE(driver()->update_accounts_called());

  FinishAccountAddition(account_id2);
  IssueAccessToken(account_id2);
  EXPECT_TRUE(driver()->update_accounts_called());

  std::vector<GCMClient::AccountTokenInfo> expected_accounts;
  expected_accounts.push_back(MakeAccountToken(account_id1));
  expected_accounts.push_back(MakeAccountToken(account_id2));
  VerifyAccountTokens(expected_accounts, driver()->accounts());
}

TEST_F(GCMAccountTrackerTest, AccountAdded) {
  tracker()->Start();
  driver()->ResetResults();

  std::string account_id1 = AddPrimaryAccount(kEmail1);
  EXPECT_FALSE(driver()->update_accounts_called());

  IssueAccessToken(account_id1);
  EXPECT_TRUE(driver()->update_accounts_called());

  std::vector<GCMClient::AccountTokenInfo> expected_accounts;
  expected_accounts.push_back(MakeAccountToken(account_id1));
  VerifyAccountTokens(expected_accounts, driver()->accounts());
}

TEST_F(GCMAccountTrackerTest, AccountRemoved) {
  std::string account_id1 = AddPrimaryAccount(kEmail1);
  std::string account_id2 = AddAccount(kEmail2);

  tracker()->Start();
  IssueAccessToken(account_id1);
  IssueAccessToken(account_id2);
  EXPECT_TRUE(driver()->update_accounts_called());

  driver()->ResetResults();
  EXPECT_FALSE(driver()->update_accounts_called());

  RemoveAccount(account_id2);
  EXPECT_TRUE(driver()->update_accounts_called());

  std::vector<GCMClient::AccountTokenInfo> expected_accounts;
  expected_accounts.push_back(MakeAccountToken(account_id1));
  VerifyAccountTokens(expected_accounts, driver()->accounts());
}

TEST_F(GCMAccountTrackerTest, GetTokenFailed) {
  std::string account_id1 = AddPrimaryAccount(kEmail1);
  std::string account_id2 = AddAccount(kEmail2);

  tracker()->Start();
  IssueAccessToken(account_id1);
  EXPECT_FALSE(driver()->update_accounts_called());

  IssueError(account_id2);

  // Failed token is not retried any more. Account marked as removed.
  EXPECT_EQ(0UL, tracker()->get_pending_token_request_count());
  EXPECT_TRUE(driver()->update_accounts_called());

  std::vector<GCMClient::AccountTokenInfo> expected_accounts;
  expected_accounts.push_back(MakeAccountToken(account_id1));
  VerifyAccountTokens(expected_accounts, driver()->accounts());
}

TEST_F(GCMAccountTrackerTest, GetTokenFailedAccountRemoved) {
  std::string account_id1 = AddPrimaryAccount(kEmail1);
  std::string account_id2 = AddAccount(kEmail2);

  tracker()->Start();
  IssueAccessToken(account_id1);

  driver()->ResetResults();
  RemoveAccount(account_id2);
  IssueError(account_id2);

  EXPECT_TRUE(driver()->update_accounts_called());

  std::vector<GCMClient::AccountTokenInfo> expected_accounts;
  expected_accounts.push_back(MakeAccountToken(account_id1));
  VerifyAccountTokens(expected_accounts, driver()->accounts());
}

TEST_F(GCMAccountTrackerTest, AccountRemovedWhileRequestsPending) {
  std::string account_id1 = AddPrimaryAccount(kEmail1);
  std::string account_id2 = AddAccount(kEmail2);

  tracker()->Start();
  IssueAccessToken(account_id1);
  EXPECT_FALSE(driver()->update_accounts_called());

  RemoveAccount(account_id2);
  IssueAccessToken(account_id2);
  EXPECT_TRUE(driver()->update_accounts_called());

  std::vector<GCMClient::AccountTokenInfo> expected_accounts;
  expected_accounts.push_back(MakeAccountToken(account_id1));
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
  std::string account_id1 = StartPrimaryAccountAddition(kEmail1);
  tracker()->Start();
  FinishAccountAddition(account_id1);

  EXPECT_EQ(0UL, tracker()->get_pending_token_request_count());
  driver()->SetConnected(true);

  EXPECT_EQ(1UL, tracker()->get_pending_token_request_count());
}

TEST_F(GCMAccountTrackerTest, InvalidateExpiredTokens) {
  std::string account_id1 = StartPrimaryAccountAddition(kEmail1);
  std::string account_id2 = StartAccountAddition(kEmail2);
  tracker()->Start();
  FinishAccountAddition(account_id1);
  FinishAccountAddition(account_id2);

  EXPECT_EQ(2UL, tracker()->get_pending_token_request_count());

  IssueExpiredAccessToken(account_id1);
  IssueAccessToken(account_id2);
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
  std::string account_id1 = StartPrimaryAccountAddition(kEmail1);
  FinishAccountAddition(account_id1);
  EXPECT_TRUE(IsFetchingRequired());

  driver()->SetConnected(true);
  EXPECT_FALSE(IsFetchingRequired());  // Indicates that fetching has started.
  IssueAccessToken(account_id1);
  EXPECT_FALSE(IsFetchingRequired());

  std::string account_id2 = StartAccountAddition(kEmail2);
  FinishAccountAddition(account_id2);
  EXPECT_FALSE(IsFetchingRequired());  // Indicates that fetching has started.

  // Disconnect the driver again so that the access token request being
  // fulfilled doesn't immediately cause another access token request (which
  // then would cause IsFetchingRequired() to be false, preventing us from
  // distinguishing this case from the case where IsFetchingRequired() is false
  // because GCMAccountTracker didn't detect that a new access token needs to be
  // fetched).
  driver()->SetConnected(false);
  IssueExpiredAccessToken(account_id2);

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

  std::string account_id1 = AddPrimaryAccount(kEmail1);
  IssueAccessToken(account_id1);
  driver()->ResetResults();
  // Reporting was triggered, which means testing for required will give false,
  // but we have the update call.
  RemoveAccount(account_id1);
  EXPECT_TRUE(driver()->update_accounts_called());
  EXPECT_FALSE(IsTokenReportingRequired());
}

// TODO(fgorski): Add test for adding account after removal >> make sure it does
// not mark removal.

}  // namespace gcm
