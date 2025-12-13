// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_state_fetcher.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/tribool.h"

namespace {
constexpr char kTestEmail[] = "test@example.com";
}  // namespace

class AccountStateFetcherTest : public testing::Test {
 public:
  AccountStateFetcherTest() = default;

  void SetUp() override {
    test_account_ = identity_test_env_.MakeAccountAvailable(kTestEmail);
  }

  // Helper to set the value that `get_account_state_callback` will return.
  void set_account_info_state(signin::Tribool state) {
    account_info_state_ = state;
    // Update a test account field, so that subsequent calls to
    // UpdateAccountInfoForAccount will trigger the extended info account
    // update.
    test_account_.full_name = std::string("name") + TriboolToString(state);
  }

  // The callback passed to the fetcher to get the account info state.
  base::RepeatingCallback<signin::Tribool(const AccountInfo&)>
  get_account_state_callback() {
    return base::BindRepeating(
        [](signin::Tribool* state, const AccountInfo&) { return *state; },
        &account_info_state_);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  AccountInfo test_account_;

  // The current state of the account info we are awaiting for.
  signin::Tribool account_info_state_ = signin::Tribool::kUnknown;
};

// Tests that the callback is executed immediately if the account info is already
// known.
TEST_F(AccountStateFetcherTest, RunsCallbackWhenAccountInfoAlreadyKnown) {
  set_account_info_state(signin::Tribool::kTrue);
  base::test::TestFuture<signin::Tribool> info_fetched_callback;

  AccountStateFetcher fetcher(identity_test_env_.identity_manager(),
                             test_account_, get_account_state_callback(),
                             info_fetched_callback.GetCallback());
  fetcher.FetchAccountInfo();
  EXPECT_TRUE(info_fetched_callback.Wait());
  EXPECT_EQ(signin::Tribool::kTrue,
            info_fetched_callback.Get<signin::Tribool>());
}

// Tests that the fetcher waits for an account info update.
TEST_F(AccountStateFetcherTest, RunsCallbackWhenAccountInfoBecomesAvailable) {
  base::test::TestFuture<signin::Tribool> info_fetched_callback;
  AccountStateFetcher fetcher(identity_test_env_.identity_manager(),
                             test_account_, get_account_state_callback(),
                             info_fetched_callback.GetCallback());
  // Start the fetching. No callback is executed because the
  // account info's value is unknown.
  fetcher.FetchAccountInfo();

  // Setting the account info triggers the callback.
  set_account_info_state(signin::Tribool::kTrue);
  identity_test_env_.UpdateAccountInfoForAccount(test_account_);
  EXPECT_TRUE(info_fetched_callback.Wait());
  EXPECT_EQ(signin::Tribool::kTrue,
            info_fetched_callback.Get<signin::Tribool>());
}

// Tests that the fetcher correctly handles a timeout.
TEST_F(AccountStateFetcherTest, RunsCallbackOnTimeout) {
  base::test::TestFuture<signin::Tribool> info_fetched_callback;
  AccountStateFetcher fetcher(identity_test_env_.identity_manager(),
                             test_account_, get_account_state_callback(),
                             info_fetched_callback.GetCallback());
  // Start the fetching. No callback is executed because the
  // account info's value is unknown.
  fetcher.FetchAccountInfo();

  // Expect the callback to be called after the timeout.
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(info_fetched_callback.Wait());
  EXPECT_EQ(signin::Tribool::kUnknown,
            info_fetched_callback.Get<signin::Tribool>());
}

TEST_F(AccountStateFetcherTest, IgnoresUpdateForOtherAccount) {
  bool is_cb_executed = false;
  base::OnceCallback<void(signin::Tribool)> callback =
      base::BindLambdaForTesting(
          [&](signin::Tribool) { is_cb_executed = true; });
  std::unique_ptr<AccountStateFetcher> fetcher(
      std::make_unique<AccountStateFetcher>(
          identity_test_env_.identity_manager(), test_account_,
          get_account_state_callback(), std::move(callback)));

  fetcher->FetchAccountInfo();

  set_account_info_state(signin::Tribool::kTrue);
  // Simulate an update for an unrelated account.
  AccountInfo other_account =
      identity_test_env_.MakeAccountAvailable("other@example.com");
  identity_test_env_.UpdateAccountInfoForAccount(other_account);

  // The callback has not been executed.
  EXPECT_FALSE(is_cb_executed);
}
