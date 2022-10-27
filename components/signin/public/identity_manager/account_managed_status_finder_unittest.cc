// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_managed_status_finder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

TEST(AccountManagedStatusFinderTest, IsNonEnterpriseUser) {
  // List of example emails that are not enterprise users.
  static const char* kNonEnterpriseUsers[] = {
      "fizz@aol.com",       "foo@gmail.com",         "bar@googlemail.com",
      "baz@hotmail.it",     "baz@hotmail.co.uk",     "baz@hotmail.com.tw",
      "user@msn.com",       "another_user@live.com", "foo@qq.com",
      "i_love@yahoo.com",   "i_love@yahoo.com.tw",   "i_love@yahoo.jp",
      "i_love@yahoo.co.uk", "user@yandex.ru"};

  // List of example emails that are potential enterprise users.
  static const char* kEnterpriseUsers[] = {
      "foo@google.com",
      "chrome_rules@chromium.org",
      "user@hotmail.enterprise.com",
      "user@unknown-domain.asdf",
  };

  for (const char* username : kNonEnterpriseUsers) {
    EXPECT_TRUE(AccountManagedStatusFinder::IsNonEnterpriseUser(username))
        << "IsNonEnterpriseUser returned false for " << username;
  }
  for (const char* username : kEnterpriseUsers) {
    EXPECT_FALSE(AccountManagedStatusFinder::IsNonEnterpriseUser(username))
        << "IsNonEnterpriseUser returned true for " << username;
  }
}

}  // namespace signin
