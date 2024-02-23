// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_
#define CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/types/strong_alias.h"
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

// A unique prefix identifier for a household (containing parents and children)
// from source chrome/browser/internal/resources/signin/test_accounts.json.
using FamilyIdentifier =
    base::StrongAlias<class FamilyIdentifierTag, std::string>;

// A LiveTest which assumes a specific structure of provided test accounts,
// which are forming a family:
// * head of household,
// * child.
class FamilyLiveTest : public signin::test::LiveTest {
 public:
  FamilyLiveTest() = delete;
  // Navigation will be allowed to extra hosts.
  explicit FamilyLiveTest(FamilyIdentifier family_identifier);
  // The provided family identifier will be used to select the test accounts.
  // Navigation will be allowed to extra hosts.
  FamilyLiveTest(FamilyIdentifier family_identifier,
                 const std::vector<std::string>& extra_enabled_hosts);

  ~FamilyLiveTest() override;

  // Turns on sync and closes auxiliary tabs.
  static void TurnOnSyncFor(FamilyMember& member);

 protected:
  void SetUp() override;
  void SetUpOnMainThread() override;
  void SetUpInProcessBrowserTestFixture() override;

  // Creates the GURL from the `url_spec` and ensures that the host part was
  // explicitly added to `extra_enabled_hosts`.
  GURL GetRoutedUrl(std::string_view url_spec) const;

  FamilyMember& head_of_household() {
    CHECK(head_of_household_) << "No head of household found in family: " +
                                     std::string(family_identifier_->data());
    return *head_of_household_;
  }
  FamilyMember& child() {
    CHECK(child_) << "No child found in family: " +
                         std::string(family_identifier_->data());
    return *child_;
  }

 private:
  // Extracts requested account, which must exist.
  signin::test::TestAccount GetTestAccount(std::string_view account_name) const;
  // Checks if the requested account exists.
  bool AccountExists(std::string_view account_name) const;

  // Creates a new browser signed in to the specified account, which must
  // exist.
  std::unique_ptr<FamilyMember> MakeSignedInBrowser(
      std::string_view account_name);

  FamilyIdentifier family_identifier_;
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
  InteractiveFamilyLiveTest() = delete;
  explicit InteractiveFamilyLiveTest(FamilyIdentifier family_identifier);
  InteractiveFamilyLiveTest(
      FamilyIdentifier family_identifier,
      const std::vector<std::string>& extra_enabled_hosts);

 protected:
  // A collection of functions that return a MultiStep for the RunTestSequence
  // that: 1) Issue a rpc by the `head_of_household()` that requests seeding the
  // chrome test state of family link settings for the `child()`  account,
  //
  // 2) Wait for that request to be fully processed through the Google
  // infrastructure, including being propagated to the browser associated with
  // the `child()` account.
  //
  // Usage:
  //
  // DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ChromeTestStateSeededObserver,
  // kResetObserver);
  // DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ChromeTestStateSeededObserver,
  // kDefineObserver);
  // ...
  // RunTestSequence(
  //   ResetChromeTestState(kResetObserver),
  //   ...
  //   DefineChromeTestState(kDefineObserver,
  //   {GetRoutedUrl("http://example.com")},
  //   {GetRoutedUrl("http://mature.example.com")}),
  //   ...
  // )
  ui::test::internal::InteractiveTestPrivate::MultiStep DefineChromeTestState(
      ui::test::StateIdentifier<ChromeTestStateObserver> id,
      const std::vector<GURL>& allowed_urls,
      const std::vector<GURL>& blocked_urls);
  ui::test::internal::InteractiveTestPrivate::MultiStep ResetChromeTestState(
      ui::test::StateIdentifier<ChromeTestStateObserver> id);
};

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_
