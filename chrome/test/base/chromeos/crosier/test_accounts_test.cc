// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/test_accounts.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace crosier {

TEST(TestAccountTest, Get) {
  std::string email;
  std::string password;

  GetGaiaTestAccount(email, password);
  EXPECT_TRUE(!email.empty());
  EXPECT_TRUE(!password.empty());
}

TEST(TestFamilyAccountsTest, Get) {
  FamilyAccounts accounts = GetFamilyTestAccounts();
  EXPECT_TRUE(!accounts.unicorn.email.empty());
  EXPECT_TRUE(!accounts.unicorn.password.empty());
  EXPECT_TRUE(!accounts.geller.email.empty());
  EXPECT_TRUE(!accounts.geller.password.empty());
  EXPECT_TRUE(!accounts.griffin.email.empty());
  EXPECT_TRUE(!accounts.griffin.password.empty());
  EXPECT_TRUE(!accounts.parent.email.empty());
  EXPECT_TRUE(!accounts.parent.password.empty());
}

}  // namespace crosier
