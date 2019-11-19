// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/keychain_password_mac.h"

#include "build/build_config.h"
#include "crypto/mock_apple_keychain.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_IOS)
#include "components/os_crypt/encryption_key_creation_util_ios.h"
#else
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/os_crypt/encryption_key_creation_util_mac.h"
#include "components/os_crypt/os_crypt_pref_names_mac.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#endif

namespace {

using crypto::MockAppleKeychain;
using os_crypt::EncryptionKeyCreationUtil;
using GetKeyAction = EncryptionKeyCreationUtil::GetKeyAction;

// An environment for KeychainPassword which initializes mock keychain with
// the given value that is going to be returned when accessing the Keychain and
// key creation utility with the given initial state (was the encryption key
// previously added to the Keychain or not).
class KeychainPasswordEnvironment {
 public:
  // |keychain_result| is the value that is going to be returned when accessing
  // the Keychain. If |is_key_already_created| is true, a preference that
  // indicates if the encryption key was created in the past will be set.
  KeychainPasswordEnvironment(OSStatus keychain_result,
                              bool is_key_already_created);

  ~KeychainPasswordEnvironment() = default;

  MockAppleKeychain& keychain() { return keychain_; }

  std::string GetPassword() const { return keychain_password_->GetPassword(); }

#if !defined(OS_IOS)
  // Returns true if the preference for key creation is set.
  bool IsKeyCreationPrefSet() const {
    return testing_local_state_.GetBoolean(os_crypt::prefs::kKeyCreated);
  }
#endif

 private:
  MockAppleKeychain keychain_;
  std::unique_ptr<KeychainPassword> keychain_password_;
#if !defined(OS_IOS)
  TestingPrefServiceSimple testing_local_state_;
#endif
};

KeychainPasswordEnvironment::KeychainPasswordEnvironment(
    OSStatus keychain_find_generic_result,
    bool is_key_already_created) {
  // Set the value that keychain is going to return.
  keychain_.set_find_generic_result(keychain_find_generic_result);

#if !defined(OS_IOS)
  // Initialize the preference on Mac.
  testing_local_state_.registry()->RegisterBooleanPref(
      os_crypt::prefs::kKeyCreated, false);
  if (is_key_already_created)
    testing_local_state_.SetBoolean(os_crypt::prefs::kKeyCreated, true);
#endif

// Initialize encryption key creation utility.
#if defined(OS_IOS)
  std::unique_ptr<EncryptionKeyCreationUtil> util =
      std::make_unique<os_crypt::EncryptionKeyCreationUtilIOS>();
#else
  std::unique_ptr<EncryptionKeyCreationUtil> util =
      std::make_unique<os_crypt::EncryptionKeyCreationUtilMac>(
          &testing_local_state_, base::ThreadTaskRunnerHandle::Get());
#endif

  // Initialize keychain password.
  keychain_password_ =
      std::make_unique<KeychainPassword>(keychain_, std::move(util));
}

class KeychainPasswordTest : public testing::Test {
 protected:
  KeychainPasswordTest() = default;

#if !defined(OS_IOS)
  // Waits until all tasks in the task runner's queue are finished.
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  void ExpectUniqueGetKeyAction(GetKeyAction action) {
    histogram_tester_.ExpectUniqueSample("OSCrypt.GetEncryptionKeyAction",
                                         action, 1);
  }
#endif

 private:
#if !defined(OS_IOS)
  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_;
#endif

  DISALLOW_COPY_AND_ASSIGN(KeychainPasswordTest);
};

// Test that if we have an existing password in the Keychain and we are
// authorized by the user to read it then we get it back correctly.
TEST_F(KeychainPasswordTest, FindPasswordSuccess) {
  KeychainPasswordEnvironment environment(noErr, true);
  EXPECT_FALSE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  EXPECT_EQ(0, environment.keychain().password_data_count());
}

// Test that if we do not have an existing password in the Keychain then it
// gets added successfully and returned.
TEST_F(KeychainPasswordTest, FindPasswordNotFound) {
  KeychainPasswordEnvironment environment(errSecItemNotFound, false);
  EXPECT_EQ(24U, environment.GetPassword().length());
  EXPECT_TRUE(environment.keychain().called_add_generic());
  EXPECT_EQ(0, environment.keychain().password_data_count());
}

// Test that if get denied access by the user then we return an empty password.
// And we should not try to add one.
TEST_F(KeychainPasswordTest, FindPasswordNotAuthorized) {
  KeychainPasswordEnvironment environment(errSecAuthFailed, false);
  EXPECT_TRUE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  EXPECT_EQ(0, environment.keychain().password_data_count());
#if !defined(OS_IOS)
  // The key creation pref shouldn't be set.
  RunUntilIdle();
  EXPECT_FALSE(environment.IsKeyCreationPrefSet());
#endif
}

// Test that if some random other error happens then we return an empty
// password, and we should not try to add one.
TEST_F(KeychainPasswordTest, FindPasswordOtherError) {
  KeychainPasswordEnvironment environment(errSecNotAvailable, false);
  EXPECT_TRUE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  EXPECT_EQ(0, environment.keychain().password_data_count());
#if !defined(OS_IOS)
  // The key creation pref shouldn't be set.
  RunUntilIdle();
  EXPECT_FALSE(environment.IsKeyCreationPrefSet());
#endif
}

// Test that subsequent additions to the keychain give different passwords.
TEST_F(KeychainPasswordTest, PasswordsDiffer) {
  KeychainPasswordEnvironment environment1(errSecItemNotFound, false);
  std::string password1 = environment1.GetPassword();
  EXPECT_FALSE(password1.empty());
  EXPECT_TRUE(environment1.keychain().called_add_generic());
  EXPECT_EQ(0, environment1.keychain().password_data_count());

  KeychainPasswordEnvironment environment2(errSecItemNotFound, false);
  std::string password2 = environment2.GetPassword();
  EXPECT_FALSE(password2.empty());
  EXPECT_TRUE(environment2.keychain().called_add_generic());
  EXPECT_EQ(0, environment2.keychain().password_data_count());

  // And finally check that the passwords are different.
  EXPECT_NE(password1, password2);
}

#if !defined(OS_IOS)
// Test that a key is overwritten even if it was created in the past.
TEST_F(KeychainPasswordTest, OverwriteKey) {
  KeychainPasswordEnvironment environment(errSecItemNotFound, true);
  EXPECT_FALSE(environment.GetPassword().empty());
  EXPECT_TRUE(environment.keychain().called_add_generic());
  RunUntilIdle();
  ExpectUniqueGetKeyAction(GetKeyAction::kKeyPotentiallyOverwritten);
}

// Test that a new key is added if one doesn't already exist in the Keychain,
// and that the key creation preference is set.
TEST_F(KeychainPasswordTest, AddNewKey) {
  KeychainPasswordEnvironment environment(errSecItemNotFound, false);

  EXPECT_FALSE(environment.GetPassword().empty());
  EXPECT_TRUE(environment.keychain().called_add_generic());
  RunUntilIdle();
  EXPECT_TRUE(environment.IsKeyCreationPrefSet());
  ExpectUniqueGetKeyAction(GetKeyAction::kNewKeyAddedToKeychain);
}

// Test that the key creation preference is set when successfully accessing the
// key from the Keychain for the first time.
TEST_F(KeychainPasswordTest, FindKeyTheFirstTime) {
  KeychainPasswordEnvironment environment(noErr, false);

  EXPECT_FALSE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  RunUntilIdle();
  EXPECT_TRUE(environment.IsKeyCreationPrefSet());
  ExpectUniqueGetKeyAction(GetKeyAction::kKeyFoundFirstTime);
}

// Test that the key creation preference is not set, that an empty password is
// returned and no password is added to the Keychain if an error other than
// errSecItemNotFound is returned by the Keychain.
TEST_F(KeychainPasswordTest, LookupOtherError) {
  KeychainPasswordEnvironment environment(errSecNotAvailable, false);

  EXPECT_TRUE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  RunUntilIdle();
  EXPECT_FALSE(environment.IsKeyCreationPrefSet());
  ExpectUniqueGetKeyAction(GetKeyAction::kKeychainLookupFailed);
}

TEST_F(KeychainPasswordTest, KeyFoundSecondTime) {
  KeychainPasswordEnvironment environment(noErr, true);

  EXPECT_FALSE(environment.GetPassword().empty());
  EXPECT_FALSE(environment.keychain().called_add_generic());
  RunUntilIdle();
  ExpectUniqueGetKeyAction(GetKeyAction::kKeyFound);
}
#endif  // !defined(OS_IOS)

}  // namespace
