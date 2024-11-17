// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TEST_ACCOUNTS_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TEST_ACCOUNTS_H_

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace base {
class FilePath;
}

namespace signin {

struct TestAccountSigninCredentials {
  std::string user;
  std::string password;
};

// A repository of test account credentials, accessible by their labels. Reads
// the credentials specific to a platform.
class TestAccountsConfig {
 public:
  TestAccountsConfig();
  ~TestAccountsConfig();
  TestAccountsConfig(const TestAccountsConfig&) = delete;
  TestAccountsConfig& operator=(const TestAccountsConfig&) = delete;

  // Inits this configuration from a JSON file.
  // Expected format maps accounts into list of credentials per supported
  // platform, eg:
  // { "account": {"win": {"user": "username", "password":
  // "some_password"}}}.
  // See unit tests for details.
  [[nodiscard]] bool Init(const base::FilePath& config_path);

  // Returns an account selected by `name` if it exists or nothing
  std::optional<TestAccountSigninCredentials> GetAccount(
      std::string_view name) const;

 private:
  std::map<std::string, TestAccountSigninCredentials, std::less<>>
      all_accounts_;
};
}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_TEST_ACCOUNTS_H_
