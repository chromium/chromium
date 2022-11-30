// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/browser_policy_connector.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(BrowserPolicyConnectorTest, IsNonEnterpriseUser) {
  // List of example emails that are not enterprise users.
  static const char* kNonEnterpriseUsers[] = {
    "fizz@aol.com",
    "foo@gmail.com",
    "bar@googlemail.com",
    "baz@hotmail.it",
    "baz@hotmail.co.uk",
    "baz@hotmail.com.tw",
    "user@msn.com",
    "another_user@live.com",
    "foo@qq.com",
    "i_love@yahoo.com",
    "i_love@yahoo.com.tw",
    "i_love@yahoo.jp",
    "i_love@yahoo.co.uk",
    "user@yandex.ru"
  };

  // List of example emails that are potential enterprise users.
  static const char* kEnterpriseUsers[] = {
    "foo@google.com",
    "chrome_rules@chromium.org",
    "user@hotmail.enterprise.com",
  };

  for (unsigned int i = 0; i < std::size(kNonEnterpriseUsers); ++i) {
    std::string username(kNonEnterpriseUsers[i]);
    EXPECT_TRUE(BrowserPolicyConnector::IsNonEnterpriseUser(username)) <<
        "IsNonEnterpriseUser returned false for " << username;
  }
  for (unsigned int i = 0; i < std::size(kEnterpriseUsers); ++i) {
    std::string username(kEnterpriseUsers[i]);
    EXPECT_FALSE(BrowserPolicyConnector::IsNonEnterpriseUser(username)) <<
        "IsNonEnterpriseUser returned true for " << username;
  }
}

}  // namespace policy
