// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_
#define CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_

#include <memory>
#include <string>
#include <string_view>

#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/supervised_user/browser_user.h"
#include "components/signin/public/identity_manager/test_accounts.h"
#include "components/supervised_user/test_support/family_link_settings_state_management.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/base/interaction/state_observer.h"
#include "url/gurl.h"

namespace supervised_user {

// Refers to the family prefix in resources/signin/test_accounts.json
inline constexpr char kAccountRepositoryPath[] =
    "supervised-tests-account-repository-path";
inline constexpr char kFamilyFeatureIdentifierSwitch[] =
    "supervised-tests-family-identifier";

// Alternatively, use these two to provide head of household's and child's
// credentials directly, in <username>:<password> syntax (colon separated).
inline constexpr char kHeadOfHouseholdCredentialsSwitch[] =
    "supervised-tests-hoh-credentials";
inline constexpr char kChildCredentialsSwitch[] =
    "supervised-tests-child-credentials";

// A LiveTest which assumes a specific structure of provided test accounts,
// which are forming a family:
// * head of household,
// * child.
// The family is read from command line switch at kFamilyIdentifierSwitch.
class FamilyLiveTest : public signin::test::LiveTest {
 public:
  // Determines which user will call the rpc.
  enum class RpcMode : int {
    // Rpc mode will mimic real life: there will be two browsers, one for a
    // supervisor (typically, head of household or parent), and one for
    // the supervised user (child).
    kProd = 0,
    // Rpc mode will take advantage of test backend impersonation feature, where
    // the client is only using child account, and the
    // server is impersonating the child.
    kTestImpersonation = 1,
  };

  explicit FamilyLiveTest(RpcMode rpc_mode);
  // The provided family identifier will be used to select the test accounts.
  // Navigation will be allowed to extra hosts.
  FamilyLiveTest(FamilyLiveTest::RpcMode rpc_mode,
                 const std::vector<std::string_view>& extra_enabled_hosts);

  ~FamilyLiveTest() override;

  // Turns on sync for eligible users depending on the ::rpc_mode_
  // (see ::TurnOnSyncFor).
  void TurnOnSync();

  // Turns on sync and waits for the sync subsystem to start. Manages the list
  // of open service tabs.
  void TurnOnSyncFor(BrowserUser& browser_user);

 protected:
  void SetUp() override;
  void SetUpOnMainThread() override;
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownOnMainThread() override;

  // Creates the GURL from the `url_spec` and ensures that the host part was
  // explicitly added to `extra_enabled_hosts`.
  GURL GetRoutedUrl(std::string_view url_spec) const;

  // Members of the family.
  BrowserUser& head_of_household() const;
  BrowserUser& child() const;

  // Family member that will issue rpc.
  BrowserUser& rpc_issuer() const;

 private:
  // Creates a FamilyMember entity using credentials from TestAccount.
  void SetHeadOfHousehold(const test_accounts::FamilyMember& credentials);
  void SetChild(const test_accounts::FamilyMember& credentials);

  // Extracts requested account from test_accounts.json file, which must exist.
  signin::TestAccountSigninCredentials GetAccountFromFile(
      std::string_view account_name_suffix) const;

  // Creates a new browser signed in to the specified account credentials that
  // represent a family member.
  std::unique_ptr<BrowserUser> MakeSignedInBrowser(
      const test_accounts::FamilyMember& credentials);

  // Empty, if rpc_mode_ is kImpersonation.
  std::unique_ptr<BrowserUser> head_of_household_;

  // Subject of testing.
  std::unique_ptr<BrowserUser> child_;

  // List of additional hosts that will have host resolution enabled. Host
  // resolution is configured as part of test startup.
  const std::vector<std::string> extra_enabled_hosts_;

  const RpcMode rpc_mode_;
};

std::string ToString(FamilyLiveTest::RpcMode rpc_mode);

// Fixture that combines InProcessBrowserTest with InteractiveBrowserTest,
// adding Family test related utilities.
class InteractiveFamilyLiveTest
    : public InteractiveBrowserTestT<FamilyLiveTest> {
 public:
  // Observes if the browser has reached the intended state.
  using InIntendedStateObserver = ui::test::PollingStateObserver<bool>;

  explicit InteractiveFamilyLiveTest(FamilyLiveTest::RpcMode rpc_mode);
  InteractiveFamilyLiveTest(
      FamilyLiveTest::RpcMode rpc_mode,
      const std::vector<std::string_view>& extra_enabled_hosts);

 protected:
  // After completion, supervised user settings are in `state`.
  ui::test::internal::InteractiveTestPrivate::MultiStep WaitForStateSeeding(
      ui::test::StateIdentifier<InIntendedStateObserver> id,
      const BrowserUser& browser_user,
      const FamilyLinkSettingsState& state_manager);
};

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_
