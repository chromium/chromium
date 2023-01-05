// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "chrome/browser/webauthn/local_credential_management_mac.h"

#include "build/build_config.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/fake_keychain.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

static const device::PublicKeyCredentialUserEntity kUser({1, 2, 3},
                                                         "doe@example.com",
                                                         "John Doe");
constexpr char kRpId[] = "example.com";

class LocalCredentialManagementTest : public testing::Test {
 protected:
  LocalCredentialManagementTest() = default;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  device::fido::mac::AuthenticatorConfig config_{
      .keychain_access_group = "test-keychain-access-group",
      .metadata_secret = "TestMetadataSecret"};
  LocalCredentialManagementMac local_cred_man_{config_};
  device::fido::mac::ScopedFakeKeychain keychain_{
      config_.keychain_access_group};
  device::fido::mac::TouchIdCredentialStore store_{config_};
};

TEST_F(LocalCredentialManagementTest, NoCredentials) {
  device::test::TestCallbackReceiver<bool> callback;
  local_cred_man_.HasCredentials(callback.callback());
  callback.WaitForCallback();
  EXPECT_FALSE(std::get<0>(callback.TakeResult()));

  device::test::TestCallbackReceiver<
      absl::optional<std::vector<device::DiscoverableCredentialMetadata>>>
      enumerate_callback;
  local_cred_man_.Enumerate(enumerate_callback.callback());
  enumerate_callback.WaitForCallback();
  auto result = std::get<0>(enumerate_callback.TakeResult());
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST_F(LocalCredentialManagementTest, OneCredential) {
  device::test::TestCallbackReceiver<bool> callback;
  auto credential = store_.CreateCredential(
      kRpId, kUser, device::fido::mac::TouchIdCredentialStore::kDiscoverable);
  EXPECT_TRUE(credential);
  local_cred_man_.HasCredentials(callback.callback());
  callback.WaitForCallback();
  EXPECT_TRUE(std::get<0>(callback.TakeResult()));

  device::test::TestCallbackReceiver<
      absl::optional<std::vector<device::DiscoverableCredentialMetadata>>>
      enumerate_callback;
  local_cred_man_.Enumerate(enumerate_callback.callback());
  enumerate_callback.WaitForCallback();
  const absl::optional<std::vector<device::DiscoverableCredentialMetadata>>
      result = std::get<0>(enumerate_callback.TakeResult());
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ(result->at(0).rp_id, kRpId);
}

TEST_F(LocalCredentialManagementTest, DeleteCredential) {
  device::test::TestCallbackReceiver<bool> callback;
  auto credential = store_.CreateCredential(
      kRpId, kUser, device::fido::mac::TouchIdCredentialStore::kDiscoverable);
  ASSERT_TRUE(credential);
  auto credentials = store_.FindResidentCredentials(kRpId);
  ASSERT_TRUE(credentials);
  EXPECT_EQ(credentials->size(), 1u);
  local_cred_man_.Delete(credential->first.credential_id, callback.callback());
  callback.WaitForCallback();
  EXPECT_TRUE(std::get<0>(callback.TakeResult()));
  credentials = store_.FindResidentCredentials(kRpId);
  ASSERT_TRUE(credentials);
  EXPECT_EQ(store_.FindResidentCredentials(kRpId)->size(), 0u);

  local_cred_man_.Delete(std::vector<uint8_t>{8}, callback.callback());
  callback.WaitForCallback();
  EXPECT_FALSE(std::get<0>(callback.TakeResult()));
}

TEST_F(LocalCredentialManagementTest, EditCredential) {
  device::test::TestCallbackReceiver<bool> callback;
  auto credential = store_.CreateCredential(
      kRpId, kUser, device::fido::mac::TouchIdCredentialStore::kDiscoverable);
  ASSERT_TRUE(credential);
  auto credentials =
      device::fido::mac::TouchIdCredentialStore::FindCredentialsForTesting(
          config_, kRpId);
  EXPECT_EQ(credentials.size(), 1u);
  local_cred_man_.Edit(credential->first.credential_id, "new-username",
                       callback.callback());
  callback.WaitForCallback();
  EXPECT_TRUE(std::get<0>(callback.TakeResult()));

  credentials =
      device::fido::mac::TouchIdCredentialStore::FindCredentialsForTesting(
          config_, kRpId);
  EXPECT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials.front().metadata.user_name, "new-username");
}

TEST_F(LocalCredentialManagementTest, EditLongCredential) {
  device::test::TestCallbackReceiver<bool> callback;
  auto credential = store_.CreateCredential(
      kRpId, kUser, device::fido::mac::TouchIdCredentialStore::kDiscoverable);
  ASSERT_TRUE(credential);
  auto credentials =
      device::fido::mac::TouchIdCredentialStore::FindCredentialsForTesting(
          config_, kRpId);
  EXPECT_EQ(credentials.size(), 1u);
  local_cred_man_.Edit(
      credential->first.credential_id,
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
      callback.callback());
  callback.WaitForCallback();
  EXPECT_TRUE(std::get<0>(callback.TakeResult()));

  credentials =
      device::fido::mac::TouchIdCredentialStore::FindCredentialsForTesting(
          config_, kRpId);
  EXPECT_EQ(credentials.size(), 1u);
  EXPECT_EQ(
      credentials.front().metadata.user_name,
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA…");
}

TEST_F(LocalCredentialManagementTest, EditUnknownCredential) {
  device::test::TestCallbackReceiver<bool> callback;
  auto credential = store_.CreateCredential(
      kRpId, kUser, device::fido::mac::TouchIdCredentialStore::kDiscoverable);
  ASSERT_TRUE(credential);
  auto credentials =
      device::fido::mac::TouchIdCredentialStore::FindCredentialsForTesting(
          config_, kRpId);
  EXPECT_EQ(credentials.size(), 1u);
  uint8_t credential_id[] = {0xa};
  local_cred_man_.Edit(credential_id, "new-username", callback.callback());
  callback.WaitForCallback();
  EXPECT_FALSE(std::get<0>(callback.TakeResult()));
}

class ScopedMockKeychain : device::fido::mac::Keychain {
 public:
  ScopedMockKeychain() { SetInstanceOverride(this); }
  ~ScopedMockKeychain() override { ClearInstanceOverride(); }

  MOCK_METHOD(OSStatus,
              ItemCopyMatching,
              (CFDictionaryRef query, CFTypeRef* result),
              (override));
};

class MockKeychainLocalCredentialManagementTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  ScopedMockKeychain mock_keychain_;
  LocalCredentialManagementMac local_cred_man_{
      {.keychain_access_group = "test-keychain-access-group",
       .metadata_secret = "TestMetadataSecret"}};
};

// Regression test for crbug.com/1401342.
TEST_F(MockKeychainLocalCredentialManagementTest, KeychainError) {
  EXPECT_CALL(mock_keychain_, ItemCopyMatching)
      .WillOnce(testing::Return(errSecInternalComponent));
  device::test::TestCallbackReceiver<bool> callback;
  local_cred_man_.HasCredentials(callback.callback());
  callback.WaitForCallback();
  EXPECT_FALSE(std::get<0>(callback.TakeResult()));
  testing::Mock::VerifyAndClearExpectations(&mock_keychain_);

  EXPECT_CALL(mock_keychain_, ItemCopyMatching)
      .WillOnce(testing::Return(errSecInternalComponent));
  device::test::TestCallbackReceiver<
      absl::optional<std::vector<device::DiscoverableCredentialMetadata>>>
      enumerate_callback;
  local_cred_man_.Enumerate(enumerate_callback.callback());
  enumerate_callback.WaitForCallback();
  auto result = std::get<0>(enumerate_callback.TakeResult());
  EXPECT_FALSE(result.has_value());
}

}  // namespace
