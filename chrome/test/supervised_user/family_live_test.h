// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_
#define CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/e2e_tests/live_test.h"
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "chrome/test/supervised_user/family_member.h"
#include "components/supervised_user/core/common/features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "url/gurl.h"

namespace supervised_user {

// A LiveTest which assumes a specific structure of provided test accounts,
// which are forming a family:
// * head of household,
// * child.
class FamilyLiveTest : public signin::test::LiveTest {
 public:
  // Navigation will be allowed to extra hosts.
  explicit FamilyLiveTest(std::vector<std::string> extra_enabled_hosts);
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

  FamilyMember& head_of_household() { return *head_of_household_; }
  FamilyMember& child() { return *child_; }

 private:
  base::test::ScopedFeatureList features{
      supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS};

  // Extracts requested account, which must exist.
  signin::test::TestAccount GetTestAccount(std::string_view account_name) const;

  // Creates a new browser signed in to the specified account, which must
  // exist.
  std::unique_ptr<FamilyMember> MakeSignedInBrowser(
      std::string_view account_name);

  std::unique_ptr<FamilyMember> head_of_household_;
  std::unique_ptr<FamilyMember> child_;

  // List of additional hosts that will have host resolution enabled. Host
  // resolution is configured as part of test startup.
  std::vector<std::string> extra_enabled_hosts_;
};

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_FAMILY_LIVE_TEST_H_
