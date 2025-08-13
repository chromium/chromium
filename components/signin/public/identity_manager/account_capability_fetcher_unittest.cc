// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_capability_fetcher.h"

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

class AccountCapabilityFetcherTest : public testing::Test {
 public:
  AccountCapabilityFetcherTest() = default;

  void SetUp() override {
    test_account_ = identity_test_env_.MakeAccountAvailable(kTestEmail);
  }

  // Helper to set the value that `get_capability_state_callback_` will return.
  void set_capability_state(signin::Tribool state) {
    capability_state_ = state;
    // Update a test account field, so that subsequent calls to
    // UpdateAccountInfoForAccount will trigger the extended info account
    // update. In production the extended account info updating would be triggered
    // by the capability update, however the capability value is mocked on this
    // test suite.
    test_account_.full_name = std::string("name") + TriboolToString(state);
  }

  // The callback passed to the fetcher to get the capability state.
  base::RepeatingCallback<signin::Tribool(const AccountInfo&)>
  get_capability_state_callback() {
    return base::BindRepeating(
        [](signin::Tribool* state, const AccountInfo&) { return *state; },
        &capability_state_);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  AccountInfo test_account_;

  // The current state of the capability.
  signin::Tribool capability_state_ = signin::Tribool::kUnknown;
};

// Tests that the callback is executed immediately if the capability is already
// known.
TEST_F(AccountCapabilityFetcherTest, RunsCallbackWhenCapabilityAlreadyKnown) {
  set_capability_state(signin::Tribool::kTrue);
  base::test::TestFuture<signin::Tribool> capability_fetched_callback;

  AccountCapabilityFetcher fetcher(identity_test_env_.identity_manager(),
                                   test_account_,
                                   get_capability_state_callback(),
                                   capability_fetched_callback.GetCallback());
  fetcher.FetchCapability();
  EXPECT_TRUE(capability_fetched_callback.Wait());
  EXPECT_EQ(signin::Tribool::kTrue,
            capability_fetched_callback.Get<signin::Tribool>());
}

// Tests that the fetcher waits for an account update.
TEST_F(AccountCapabilityFetcherTest,
       RunsCallbackWhenCapabilityBecomesAvailable) {
  base::test::TestFuture<signin::Tribool> capability_fetched_callback;
  AccountCapabilityFetcher fetcher(identity_test_env_.identity_manager(),
                                   test_account_,
                                   get_capability_state_callback(),
                                   capability_fetched_callback.GetCallback());
  // Start the fetching. No callback is executed because the
  // capability's value is unknown.
  fetcher.FetchCapability();

  // Setting the account capability triggers the callback.
  set_capability_state(signin::Tribool::kTrue);
  identity_test_env_.UpdateAccountInfoForAccount(test_account_);
  EXPECT_TRUE(capability_fetched_callback.Wait());
  EXPECT_EQ(signin::Tribool::kTrue,
            capability_fetched_callback.Get<signin::Tribool>());
}

// Tests that the fetcher correctly handles a timeout.
TEST_F(AccountCapabilityFetcherTest, RunsCallbackOnTimeout) {
  base::test::TestFuture<signin::Tribool> capability_fetched_callback;
  AccountCapabilityFetcher fetcher(identity_test_env_.identity_manager(),
                                   test_account_,
                                   get_capability_state_callback(),
                                   capability_fetched_callback.GetCallback());
  // Start the fetching. No callback is executed because the
  // capability's value is unknown.
  fetcher.FetchCapability();

  // Expect the callback to be called after the timeout.
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(capability_fetched_callback.Wait());
  EXPECT_EQ(signin::Tribool::kUnknown,
            capability_fetched_callback.Get<signin::Tribool>());
}

TEST_F(AccountCapabilityFetcherTest, IgnoresUpdateForOtherAccount) {
  bool is_cb_executed = false;
  base::OnceCallback<void(signin::Tribool)> callback =
      base::BindLambdaForTesting(
          [&](signin::Tribool) { is_cb_executed = true; });
  std::unique_ptr<AccountCapabilityFetcher> fetcher(
      std::make_unique<AccountCapabilityFetcher>(
          identity_test_env_.identity_manager(), test_account_,
          get_capability_state_callback(), std::move(callback)));

  fetcher->FetchCapability();

  set_capability_state(signin::Tribool::kTrue);
  // Simulate an update for an unrelated account.
  AccountInfo other_account =
      identity_test_env_.MakeAccountAvailable("other@example.com");
  identity_test_env_.UpdateAccountInfoForAccount(other_account);

  // The callback has not been executed.
  EXPECT_FALSE(is_cb_executed);
}
