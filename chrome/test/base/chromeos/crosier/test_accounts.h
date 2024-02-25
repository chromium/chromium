// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_TEST_ACCOUNTS_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_TEST_ACCOUNTS_H_

#include <string>

namespace crosier {

// Represents data that makes up a family. It consists of credentials for
// different types of accounts and information related to account configuration.
struct FamilyTestData {
  struct User {
    std::string email;
    std::string password;
  };

  FamilyTestData();
  FamilyTestData(const FamilyTestData&);
  ~FamilyTestData();

  User parent;
  User unicorn;
  User geller;
  User griffin;

  // Mature site that is blocked for the Unicorn, Geller, and Griffin accounts
  // in this account pool.
  std::string mature_site;
};

// Randomly picks a gaia test account from the test accounts pool.
void GetGaiaTestAccount(std::string& out_email, std::string& out_password);

// Returns the set of accounts relevant to a supervised user from the test
// accounts pool.
FamilyTestData GetFamilyTestData();

}  // namespace crosier

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_TEST_ACCOUNTS_H_
