// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/signin/core/browser/account_reconcilor_delegate.h"

#include <ostream>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

struct AccountReconcilorDelegateTestParam {
  const char* chrome_accounts;
  const char* gaia_accounts;
  const char* first_account;
  const char* expected_order;
};

// clang-format off
static const AccountReconcilorDelegateTestParam kReorderParams[] = {
// | Tokens          | Cookies       | First Acc. | Expected cookies |
// |------------ Basic cases ----------------------------------------|
   // Nothing to do.
   { "A",              "A",            "A",         "A"              },
   { "ABCD",           "ABCD",         "A",         "ABCD"           },
   // Token ordering does not matter.
   { "DBCA",           "ABCD",         "A",         "ABCD"           },
   // Simple reordering of cookies.
   { "AB",             "BA",           "A",         "AB"             },
// |------------ Extra accounts in cookie ---------------------------|
   // Extra secondary account.
   { "A",              "AB",           "A",         "A"              },
   // Extra primary account.
   { "A",              "BA",           "A",         "A"              },
   // Multiple extra accounts.
   { "AE",             "ABCDEF",       "A",         "AE"             },
   { "AE",             "GABCDEF",      "A",         "AE"             },
   // C is kept in place.
   { "ACF",            "ABCDEF",       "A",         "AFC"            },
// |------------ Missing accounts in cookie -------------------------|
   // Cookie was lost.
   { "A",              "",             "A",         "A"              },
   { "ABCD",           "",             "A",         "ABCD"           },
   // New account is added.
   { "ABC",            "AB",           "A",         "ABC"            },
   { "ACB",            "AB",           "A",         "ABC"            },
   { "BAC",            "AB",           "A",         "ABC"            },
   // B kept in place.
   { "ADB",            "CB",           "A",         "ABD"            },
   // ACEG kept in place.
   { "ABCDEFGH",       "ACEG",         "A",         "ACEGBDFH"       },
   // C kept in place, but not B.
   { "ABCD",           "BC",           "A",         "ACBD"           },
   // D not kept in place.
   { "AD",             "ABCD",         "A",         "AD"             },
// |------------ Both extra accounts and missing accounts -----------|
   // Simple account mismatch.
   { "A",              "B",            "A",         "A"              },
   // ADE kept in place, BG removed.
   { "ADEH",           "ABDEG",        "A",         "AHDE"           },
   // E kept in place, BG removed, AD swapped.
   { "ADEH",           "ABDEG",        "D",         "DHAE"           },
   // Missing first account.
   { "ADE",            "BCDE",         "A",         "AED"            },
   // Three-ways swap A-B-D.
   { "ABCE",           "BCDE",         "A",         "ACBE"           },
   // Extreme example.
   { "ACJKL",          "ABCDEFGHIJ",   "A",         "AKCLJ"          },
// |------------ More than 10 accounts in chrome --------------------|
   // Trim extra accounts.
   { "ABCDEFGHIJKLM",  "ABCDEFGHIJ",   "A",         "ABCDEFGHIJ"     },
   // D missing.
   { "ABCEFGHIJKLMN",  "ABCDEFGHIJ",   "A",         "ABCKEFGHIJ"     },
   // DG missing.
   { "ABCEFHIJKLMOP",  "ABCDEFGHIJ",   "A",         "ABCKEFLHIJ"     },
   // Primary swapped in.
   { "ABCDEFGHIJKLM",  "ABCDEFGHIJ",   "K",         "KBCDEFGHIJ"     },
// |------------ More than 10 accounts in cookie --------------------|
   // Trim extra account.
   { "ABCDEFGHIJK",    "ABCDEFGHIJK",  "A",         "ABCDEFGHIJ"     },
   // Other edge cases.
   { "BE",             "ABCDEFGHIJK",  "B",         "BE"             },
   { "AE",             "ABCDEFGHIJK",  "A",         "AE"             },
   { "AK",             "ABCDEFGHIJK",  "A",         "AK"             },
   { "K",              "ABCDEFGHIJK",  "K",         "K"              },
// |------------ Missing first account ------------------------------|
   // B kept in place.
   { "AB",             "B",            "",          "BA"             },
   // BC kept in place, E removed, AD added.
   { "ABCD",           "BCE",          "",          "BCAD"           },
   // C kept in place, first account D replaced.
   { "ABC",            "DC",           "",          "ACB"            },
   // First accounts match
   { "ABC",            "AC",           "",          "ACB"            },
   // Extreme example.
   { "ACJKH",          "JBCDEFGHIR",   "",          "JACKH"          },
   // Empty chrome accounts.
   { "",               "ABC",          "",          ""               },
};
// clang-format on

// Pretty prints a AccountReconcilorDelegateTestParam. Used by gtest.
static void PrintTo(const AccountReconcilorDelegateTestParam& param,
                    ::std::ostream* os) {
  *os << "gaia_accounts: \"" << param.gaia_accounts << "\". chrome_accounts: \""
      << param.chrome_accounts << "\". first_account: \"" << param.first_account
      << "\".";
}

class AccountReconcilorDelegateTest
    : public AccountReconcilorDelegate,
      public ::testing::TestWithParam<AccountReconcilorDelegateTestParam> {
 public:
  AccountReconcilorDelegateTest() = default;
  ~AccountReconcilorDelegateTest() override = default;

  // Parses a cookie string and converts it into ListedAccounts.
  std::vector<gaia::ListedAccount> GaiaAccountsFromString(
      const std::string& account_string) {
    std::vector<gaia::ListedAccount> gaia_accounts;
    for (const char& c : account_string) {
      gaia::ListedAccount account;
      account.id = CoreAccountId::FromGaiaId(std::string(1, c));
      gaia_accounts.push_back(account);
    }
    return gaia_accounts;
  }
};

TEST_P(AccountReconcilorDelegateTest, ReorderChromeAccountsForReconcile) {
  // Decode test parameters.
  CoreAccountId first_account =
      CoreAccountId::FromGaiaId(std::string(GetParam().first_account));
  std::vector<CoreAccountId> chrome_accounts;
  for (int i = 0; GetParam().chrome_accounts[i] != '\0'; ++i) {
    chrome_accounts.push_back(CoreAccountId::FromGaiaId(
        std::string(1, GetParam().chrome_accounts[i])));
  }
  ASSERT_TRUE(first_account.empty() ||
              base::Contains(chrome_accounts, first_account))
      << "Invalid test parameter.";
  std::vector<gaia::ListedAccount> gaia_accounts =
      GaiaAccountsFromString(GetParam().gaia_accounts);

  // Reorder the accounts.
  std::vector<CoreAccountId> order = ReorderChromeAccountsForReconcile(
      chrome_accounts, first_account, gaia_accounts);

  // Check results.
  std::string order_as_string;
  for (const CoreAccountId& account : order) {
    ASSERT_EQ(1u, account.ToString().size());
    order_as_string += account.ToString();
  }
  EXPECT_EQ(GetParam().expected_order, order_as_string);

  // Check that the result is idempotent (re-ordering again is a no-op).
  EXPECT_EQ(order, ReorderChromeAccountsForReconcile(
                       chrome_accounts, first_account,
                       GaiaAccountsFromString(order_as_string)));
}

INSTANTIATE_TEST_SUITE_P(All,
                         AccountReconcilorDelegateTest,
                         ::testing::ValuesIn(kReorderParams));

}  // namespace signin
