// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/mice_account_reconcilor_delegate.h"

#include <vector>

#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

namespace {

// Returns a gaia::ListedAccount with the specified account id.
gaia::ListedAccount BuildTestListedAccount(const std::string account_id,
                                           bool valid) {
  gaia::ListedAccount account;
  account.id = CoreAccountId(account_id);
  account.valid = valid;
  return account;
}

// Returns vector of account_id created from value
std::vector<CoreAccountId> ToAccountIdList(
    const std::vector<std::string>& account_ids_value) {
  std::vector<CoreAccountId> account_ids;
  for (const auto& account_id_value : account_ids_value)
    account_ids.push_back(CoreAccountId(account_id_value));
  return account_ids;
}

}  // namespace

TEST(MiceAccountReconcilorDelegate, CalculateParametersForMultilogin) {
  MiceAccountReconcilorDelegate mice_delegate;

  const gaia::ListedAccount kA = BuildTestListedAccount("A", /*valid=*/true);
  const gaia::ListedAccount kB = BuildTestListedAccount("B", /*valid=*/false);

  struct TestParams {
    std::vector<std::string> chrome_accounts;
    std::string primary_account;
    std::vector<gaia::ListedAccount> gaia_accounts;

    std::vector<std::string> expected_accounts;
  };

  // clang-format off
  TestParams cases[] = {
  // chrome_accounts, primary_account, gaia_accounts, expected_accounts
    {{},              "",              {},            {}},
    {{},              "",              {kA, kB},      {}},
    {{"A"},           "",              {},            {"A"}},
    {{"A"},           "",              {kA, kB},      {"A"}},
    {{"A"},           "",              {kB, kA},      {"A"}},
    {{"A", "B"},      "",              {},            {"A", "B"}},
    {{"A", "B"},      "",              {kA},          {"A", "B"}},
    {{"A", "B"},      "",              {kB},          {"A", "B"}},
    {{"B", "C"},      "",              {kA},          {"B", "C"}},
    {{"A", "B"},      "",              {kA, kB},      {"A", "B"}},
    {{"A", "B"},      "",              {kB, kA},      {"A", "B"}},
    {{"B", "C"},      "",              {kA, kB},      {"B", "C"}},
    // Tests the reordering: B remains in 2nd place.
    {{"C", "D", "B"}, "",              {kA, kB},      {"C", "B", "D"}},
    // With primary account.
    {{"A", "B"},      "B",             {},            {"B", "A"}},
    {{"B", "A"},      "A",             {kB, kA},      {"A", "B"}},
    {{"A", "B"},      "B",             {kB, kA},      {"B", "A"}},
  };
  // clang-format on

  for (const auto& test : cases) {
    MultiloginParameters multilogin_parameters =
        mice_delegate.CalculateParametersForMultilogin(
            ToAccountIdList(test.chrome_accounts),
            CoreAccountId(test.primary_account), test.gaia_accounts, false,
            false);
    EXPECT_EQ(gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER,
              multilogin_parameters.mode);
    EXPECT_EQ(ToAccountIdList(test.expected_accounts),
              multilogin_parameters.accounts_to_send);
  }
}

}  // namespace signin
