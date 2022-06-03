// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/keychain_password_mac.h"

#include "build/build_config.h"
#include "crypto/mock_apple_keychain.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using crypto::MockAppleKeychain;

// An environment for KeychainPassword which initializes mock keychain with
// the given value that is going to be returned when accessing the Keychain.
class KeychainPasswordEnvironment {
 public:
  // |keychain_result| is the value that is going to be returned when accessing
  // the Keychain.
  explicit KeychainPasswordEnvironment(OSStatus keychain_result);
  ~KeychainPasswordEnvironment() = default;

  KeychainPasswordEnvironment(KeychainPasswordEnvironment&) = delete;
  KeychainPasswordEnvironment& operator=(KeychainPasswordEnvironment&) = delete;

  MockAppleKeychain& keychain() { return keychain_; }

  std::string GetPassword() const { return keychain_password_->GetPassword(); }

 private:
  MockAppleKeychain keychain_;
  std::unique_ptr<KeychainPassword> keychain_password_;
};

KeychainPasswordEnvironment::KeychainPasswordEnvironment(
    OSStatus keychain_find_generic_result) {
  // Set the value that keychain is going to return.
  keychain_.set_find_generic_result(keychain_find_generic_result);

  // Initialize keychain password.
  keychain_password_ = std::make_unique<KeychainPassword>(keychain_);
}

// Test that if we have an existing password in the Keychain and we are
// authorized by the user to read it then we get it back correctly.
TEST(KeychainPasswordTest, FindPasswordSuccess) {
  KeychainPasswordEnvironment environment(noErr);
  EXPECT_FALSE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  EXPECT_EQ(0, environment.keychain().password_data_count());
}

// Test that if we do not have an existing password in the Keychain then it
// gets added successfully and returned.
TEST(KeychainPasswordTest, FindPasswordNotFound) {
  KeychainPasswordEnvironment environment(errSecItemNotFound);
  EXPECT_EQ(24U, environment.GetPassword().length());
  EXPECT_TRUE(environment.keychain().called_add_generic());
  EXPECT_EQ(0, environment.keychain().password_data_count());
}

// Test that if get denied access by the user then we return an empty password.
// And we should not try to add one.
TEST(KeychainPasswordTest, FindPasswordNotAuthorized) {
  KeychainPasswordEnvironment environment(errSecAuthFailed);
  EXPECT_TRUE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  EXPECT_EQ(0, environment.keychain().password_data_count());
}

// Test that if some random other error happens then we return an empty
// password, and we should not try to add one.
TEST(KeychainPasswordTest, FindPasswordOtherError) {
  KeychainPasswordEnvironment environment(errSecNotAvailable);
  EXPECT_TRUE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  EXPECT_EQ(0, environment.keychain().password_data_count());
}

// Test that subsequent additions to the keychain give different passwords.
TEST(KeychainPasswordTest, PasswordsDiffer) {
  KeychainPasswordEnvironment environment1(errSecItemNotFound);
  std::string password1 = environment1.GetPassword();
  EXPECT_FALSE(password1.empty());
  EXPECT_TRUE(environment1.keychain().called_add_generic());
  EXPECT_EQ(0, environment1.keychain().password_data_count());

  KeychainPasswordEnvironment environment2(errSecItemNotFound);
  std::string password2 = environment2.GetPassword();
  EXPECT_FALSE(password2.empty());
  EXPECT_TRUE(environment2.keychain().called_add_generic());
  EXPECT_EQ(0, environment2.keychain().password_data_count());

  // And finally check that the passwords are different.
  EXPECT_NE(password1, password2);
}

}  // namespace
