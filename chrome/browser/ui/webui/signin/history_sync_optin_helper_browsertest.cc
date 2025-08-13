// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"

#include "base/test/test_future.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_capability_fetcher.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class EnterprisePolicyCapabilityObserverBrowserTest
    : public SigninBrowserTestBase {
 public:
  EnterprisePolicyCapabilityObserverBrowserTest()
      : SigninBrowserTestBase(/*use_main_profile=*/true) {}

  AccountInfo MakeAccountInfoAvailable() {
    AccountInfo account_info =
        identity_test_env()->MakeAccountAvailable("test@example.com");
    // Fill the account info, in particular for the hosted_domain field.
    account_info.full_name = "fullname";
    account_info.given_name = "givenname";
    account_info.locale = "en";
    account_info.picture_url = "https://example.com";
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
    return account_info;
  }

  // Updates the fields relating to the account management.
  void UpdateEnterprizePolicyCapabilities(AccountInfo& account_info) {
    account_info.hosted_domain = "example.com";
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_is_subject_to_account_level_enterprise_policies(true);

    CHECK(account_info.IsValid());
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
  }
};

IN_PROC_BROWSER_TEST_F(EnterprisePolicyCapabilityObserverBrowserTest,
                       ExecutesCallbackWhenCapabilityFetched) {
  base::test::TestFuture<signin::Tribool>
      enterprize_policy_capability_fetched_callback;
  AccountInfo account_info = MakeAccountInfoAvailable();

  EnterprisePolicyCapabilityObserver enterprise_policy_capability_observer(
      identity_test_env()->identity_manager(), account_info,
      enterprize_policy_capability_fetched_callback.GetCallback());
  enterprise_policy_capability_observer.FetchCapability();

  UpdateEnterprizePolicyCapabilities(account_info);
  EXPECT_TRUE(enterprize_policy_capability_fetched_callback.Wait());
  signin::Tribool is_managed_account =
      enterprize_policy_capability_fetched_callback.Get<signin::Tribool>();
  EXPECT_EQ(signin::Tribool::kTrue, is_managed_account);

  // Subsequent updates should have no impact.
  UpdateEnterprizePolicyCapabilities(account_info);
}

IN_PROC_BROWSER_TEST_F(EnterprisePolicyCapabilityObserverBrowserTest,
                       ExecutesCallbackWhenCapabilityFetchingTimesOut) {
  base::test::TestFuture<signin::Tribool>
      enterprize_policy_capability_fetched_callback;
  AccountInfo account_info = MakeAccountInfoAvailable();

  EnterprisePolicyCapabilityObserver enterprise_policy_capability_observer(
      identity_test_env()->identity_manager(), account_info,
      enterprize_policy_capability_fetched_callback.GetCallback());
  enterprise_policy_capability_observer.FetchCapability();

  enterprise_policy_capability_observer.GetAccountCapabilityFetcherForTesting()
      ->EnforceTimeoutReachedForTesting();

  EXPECT_TRUE(enterprize_policy_capability_fetched_callback.Wait());
  signin::Tribool is_managed_account =
      enterprize_policy_capability_fetched_callback.Get<signin::Tribool>();
  EXPECT_EQ(signin::Tribool::kUnknown, is_managed_account);
}
