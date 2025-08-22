// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin_helper.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_capability_fetcher.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class MockHistorySyncOptinHelperDelegate
    : public HistorySyncOptinHelper::Delegate {
 public:
  MOCK_METHOD(void, ShowHistorySyncOptinScreen, (), (override));
};
}  // namespace

class HistorySyncOptinHelperBrowserTest : public SigninBrowserTestBase {
 public:
  HistorySyncOptinHelperBrowserTest()
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
  void UpdateEnterprisePolicyCapabilities(AccountInfo& account_info,
                                          bool is_managed) {
    account_info.hosted_domain =
        is_managed ? "example.com" : signin::constants::kNoHostedDomainFound;
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_is_subject_to_account_level_enterprise_policies(is_managed);

    CHECK(account_info.IsValid());
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
  }

 private:
  base::test::ScopedFeatureList scoped_features{
      switches::kEnableHistorySyncOptin};
};

IN_PROC_BROWSER_TEST_F(
    HistorySyncOptinHelperBrowserTest,
    TriggersHistorySyncScreenWhenCapabilityFetchedForNonManagedAccount) {
  AccountInfo account_info = MakeAccountInfoAvailable();
  MockHistorySyncOptinHelperDelegate delegate;

  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).Times(1);

  HistorySyncOptinHelper history_sync_optin_helper(
      identity_test_env()->identity_manager(), account_info, &delegate);
  history_sync_optin_helper.StartHistorySyncOptinFlow();

  // This triggers the flopw that reaches the delegate's
  // `ShowHistorySyncOptinScreen`.
  UpdateEnterprisePolicyCapabilities(account_info, /*is_managed=*/false);

  // Subsequent updates should have no impact.
  UpdateEnterprisePolicyCapabilities(account_info, /*is_managed=*/false);
}

IN_PROC_BROWSER_TEST_F(
    HistorySyncOptinHelperBrowserTest,
    TriggersHistorySyncScreenWhenCapabilityFetchingTimesOut) {
  AccountInfo account_info = MakeAccountInfoAvailable();
  MockHistorySyncOptinHelperDelegate delegate;

  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).Times(0);

  HistorySyncOptinHelper history_sync_optin_helper(
      identity_test_env()->identity_manager(), account_info, &delegate);
  history_sync_optin_helper.StartHistorySyncOptinFlow();
  testing::Mock::VerifyAndClearExpectations(&delegate);

  EXPECT_CALL(delegate, ShowHistorySyncOptinScreen).Times(1);
  history_sync_optin_helper.GetAccountCapabilityFetcherForTesting()
      ->EnforceTimeoutReachedForTesting();

  // After the timeout is reached, capability updates should have no impact.
  UpdateEnterprisePolicyCapabilities(account_info, /*is_managed=*/true);
}
