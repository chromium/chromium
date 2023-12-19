// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_TEST_ACCOUNTS_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_TEST_ACCOUNTS_H_

#include <string>

namespace crosier {

// Used to store the accounts that make up a family from the test account pool.
struct FamilyAccounts {
  struct User {
    std::string email;
    std::string password;
  };

  FamilyAccounts();
  FamilyAccounts(const FamilyAccounts&);
  ~FamilyAccounts();

  User parent;
  User unicorn;
  User geller;
  User griffin;
};

// Randomly picks a gaia test account from the test accounts pool.
void GetGaiaTestAccount(std::string& out_email, std::string& out_password);

// Returns the set of accounts relevant to a supervised user login from the test
// accounts pool. This includes the supervised user accounts, parent accounts,
// and EDU accounts associated with the supervised user.
FamilyAccounts GetFamilyTestAccounts();

}  // namespace crosier

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_TEST_ACCOUNTS_H_
