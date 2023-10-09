// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/test_accounts.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace crosier {

TEST(TestAccountTest, Get) {
  std::string email;
  std::string password;

  GetGaiaTestAccount(email, password);
  EXPECT_TRUE(!email.empty());
  EXPECT_TRUE(!password.empty());
}

}  // namespace crosier
