// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_
#define CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_

#include <memory>
#include <string_view>

#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/test/supervised_user/family_member_browser.h"

namespace supervised_user {

// A LiveTest which assumes a specific structure of provided test accounts,
// which are forming a family:
// * head of household,
// * child.
class FamilyLiveTest : public signin::test::LiveTest {
 public:
  FamilyLiveTest();
  ~FamilyLiveTest() override;

 protected:
  void SetUp() override;
  void SetUpOnMainThread() override;

  FamilyMemberBrowser& head_of_household() { return *head_of_household_; }
  FamilyMemberBrowser& child() { return *head_of_household_; }

 private:
  // Extracts requested account, which must exist.
  signin::test::TestAccount GetTestAccount(std::string_view account_name) const;

  // Creates a new browser signed in to the specified account, which must
  // exist.
  std::unique_ptr<FamilyMemberBrowser> MakeSignedInBrowser(
      std::string_view account_name);

  std::unique_ptr<FamilyMemberBrowser> head_of_household_;
  std::unique_ptr<FamilyMemberBrowser> child_;
};

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_
