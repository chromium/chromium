// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/local_credential_management_mac.h"

#include <optional>

#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/apple_keychain_v2.h"
#include "crypto/scoped_fake_apple_keychain_v2.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::test::TestFuture;

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
  crypto::ScopedFakeAppleKeychainV2 keychain_{config_.keychain_access_group};
  device::fido::mac::TouchIdCredentialStore store_{config_};
};

TEST_F(LocalCredentialManagementTest, NoCredentials) {
  TestFuture<bool> future;
  local_cred_man_.HasCredentials(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get());

  TestFuture<std::optional<std::vector<device::DiscoverableCredentialMetadata>>>
      enumerate_future;
  local_cred_man_.Enumerate(enumerate_future.GetCallback());
  EXPECT_FALSE(enumerate_future.IsReady());
  EXPECT_TRUE(enumerate_future.Wait());
  auto result = enumerate_future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->empty());
}

TEST_F(LocalCredentialManagementTest, OneCredential) {
  TestFuture<bool> future;
  auto credential = store_.CreateCredential(
      kRpId, kUser, device::fido::mac::TouchIdCredentialStore::kDiscoverable);
  EXPECT_TRUE(credential);
  local_cred_man_.HasCredentials(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());

  TestFuture<std::optional<std::vector<device::DiscoverableCredentialMetadata>>>
      enumerate_future;
  local_cred_man_.Enumerate(enumerate_future.GetCallback());
  EXPECT_FALSE(enumerate_future.IsReady());
  EXPECT_TRUE(enumerate_future.Wait());
  const std::optional<std::vector<device::DiscoverableCredentialMetadata>>
      result = enumerate_future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ(result->at(0).rp_id, kRpId);
}

TEST_F(LocalCredentialManagementTest, DeleteCredential) {
  auto credential = store_.CreateCredential(
      kRpId, kUser, device::fido::mac::TouchIdCredentialStore::kDiscoverable);
  ASSERT_TRUE(credential);
  auto credentials = store_.FindResidentCredentials(kRpId);
  ASSERT_TRUE(credentials);
  EXPECT_EQ(credentials->size(), 1u);
  {
    TestFuture<bool> future;
    local_cred_man_.Delete(credential->first.credential_id,
                           future.GetCallback());
    EXPECT_FALSE(future.IsReady());
    EXPECT_TRUE(future.Wait());
    EXPECT_TRUE(future.Get());
  }
  credentials = store_.FindResidentCredentials(kRpId);
  ASSERT_TRUE(credentials);
  EXPECT_EQ(store_.FindResidentCredentials(kRpId)->size(), 0u);

  {
    TestFuture<bool> future;
    local_cred_man_.Delete(std::vector<uint8_t>{8}, future.GetCallback());
    EXPECT_FALSE(future.IsReady());
    EXPECT_TRUE(future.Wait());
    EXPECT_FALSE(future.Get());
  }
}

TEST_F(LocalCredentialManagementTest, EditCredential) {
  TestFuture<bool> future;
  auto credential = store_.CreateCredential(
      kRpId, kUser, device::fido::mac::TouchIdCredentialStore::kDiscoverable);
  ASSERT_TRUE(credential);
  auto credentials =
      device::fido::mac::TouchIdCredentialStore::FindCredentialsForTesting(
          config_, kRpId);
  EXPECT_EQ(credentials.size(), 1u);
  local_cred_man_.Edit(credential->first.credential_id, "new-username",
                       future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());

  credentials =
      device::fido::mac::TouchIdCredentialStore::FindCredentialsForTesting(
          config_, kRpId);
  EXPECT_EQ(credentials.size(), 1u);
  EXPECT_EQ(credentials.front().metadata.user_name, "new-username");
}

TEST_F(LocalCredentialManagementTest, EditLongCredential) {
  TestFuture<bool> future;
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
      future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get());

  credentials =
      device::fido::mac::TouchIdCredentialStore::FindCredentialsForTesting(
          config_, kRpId);
  EXPECT_EQ(credentials.size(), 1u);
  EXPECT_EQ(
      credentials.front().metadata.user_name,
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA…");
}

TEST_F(LocalCredentialManagementTest, EditUnknownCredential) {
  TestFuture<bool> future;
  auto credential = store_.CreateCredential(
      kRpId, kUser, device::fido::mac::TouchIdCredentialStore::kDiscoverable);
  ASSERT_TRUE(credential);
  auto credentials =
      device::fido::mac::TouchIdCredentialStore::FindCredentialsForTesting(
          config_, kRpId);
  EXPECT_EQ(credentials.size(), 1u);
  uint8_t credential_id[] = {0xa};
  local_cred_man_.Edit(credential_id, "new-username", future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get());
}

class ScopedMockKeychain : crypto::AppleKeychainV2 {
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
  TestFuture<bool> future;
  local_cred_man_.HasCredentials(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(future.Get());
  testing::Mock::VerifyAndClearExpectations(&mock_keychain_);

  EXPECT_CALL(mock_keychain_, ItemCopyMatching)
      .WillOnce(testing::Return(errSecInternalComponent));
  TestFuture<std::optional<std::vector<device::DiscoverableCredentialMetadata>>>
      enumerate_future;
  local_cred_man_.Enumerate(enumerate_future.GetCallback());
  EXPECT_FALSE(enumerate_future.IsReady());
  EXPECT_TRUE(enumerate_future.Wait());
  EXPECT_FALSE(enumerate_future.Get());
}

}  // namespace
