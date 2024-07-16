// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_
#define CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/supervised_user/family_member.h"
#include "chrome/test/supervised_user/test_state_seeded_observer.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interactive_test_internal.h"
#include "ui/base/interaction/state_observer.h"
#include "url/gurl.h"

namespace supervised_user {

// Refers to the family prefix in resources/signin/test_accounts.json
const char* const kFamilyIdentifierSwitch =
    "supervised-tests-family-identifier";

// Alternatively, use these two to provide head of household's and child's
// credentials directly, in <username>:<password> syntax (colon separated).
const char* const kHeadOfHouseholdCredentialsSwitch =
    "supervised-tests-hoh-credentials";
const char* const kChildCredentialsSwitch =
    "supervised-tests-child-credentials";

// A LiveTest which assumes a specific structure of provided test accounts,
// which are forming a family:
// * head of household,
// * child.
// The family is read from command line switch at kFamilyIdentifierSwitch.
class FamilyLiveTest : public signin::test::LiveTest {
 public:
  FamilyLiveTest();
  // The provided family identifier will be used to select the test accounts.
  // Navigation will be allowed to extra hosts.
  explicit FamilyLiveTest(const std::vector<std::string>& extra_enabled_hosts);

  ~FamilyLiveTest() override;

  // Turns on sync and waits for the sync subsystem to start. Manages the list
  // of open service tabs.
  void TurnOnSyncFor(FamilyMember& member);

 protected:
  void SetUp() override;
  void SetUpOnMainThread() override;
  void SetUpInProcessBrowserTestFixture() override;

  // Creates the GURL from the `url_spec` and ensures that the host part was
  // explicitly added to `extra_enabled_hosts`.
  GURL GetRoutedUrl(std::string_view url_spec) const;

  FamilyMember& head_of_household() const;
  FamilyMember& child() const;

 private:
  void SetFamilyMembers(const ::signin::test::TestAccount& head_of_household,
                        const ::signin::test::TestAccount& child);

  // Extracts requested account from test_accounts.json file, which must exist.
  signin::test::TestAccount GetAccountFromFile(
      std::string_view account_name_suffix) const;

  // Creates a new browser signed in to the specified account
  std::unique_ptr<FamilyMember> MakeSignedInBrowser(
      const signin::test::TestAccount& account);

  std::unique_ptr<FamilyMember> head_of_household_;
  std::unique_ptr<FamilyMember> child_;

  // List of additional hosts that will have host resolution enabled. Host
  // resolution is configured as part of test startup.
  std::vector<std::string> extra_enabled_hosts_;
};

// Fixture that combines InProcessBrowserTest with InteractiveBrowserTest,
// adding Family test related utilities.
class InteractiveFamilyLiveTest
    : public InteractiveBrowserTestT<FamilyLiveTest> {
 public:
  InteractiveFamilyLiveTest();
  explicit InteractiveFamilyLiveTest(
      const std::vector<std::string>& extra_enabled_hosts);

 protected:
  // After completion, supervised user settings are in `state`.
  ui::test::internal::InteractiveTestPrivate::MultiStep WaitForStateSeeding(
      ui::test::StateIdentifier<BrowserState::Observer> id,
      const FamilyMember& rpc_issuer,
      const FamilyMember& browser_user,
      const BrowserState& state);
};

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_
