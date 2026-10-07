// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/icloud_recovery_util.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/trusted_vault/icloud_recovery_key_mac.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_crypto.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {
namespace {

trusted_vault::MemberKeys CreateWrappedMemberKey(
    const trusted_vault::SecureBoxPublicKey& public_key,
    base::span<const uint8_t> secret,
    int version) {
  std::vector<uint8_t> wrapped_key =
      trusted_vault::ComputeTrustedVaultWrappedKey(public_key, secret);
  auto proof = trusted_vault::ComputeMemberProof(public_key, secret);
  return trusted_vault::MemberKeys(
      version, std::move(wrapped_key),
      std::vector<uint8_t>(proof.begin(), proof.end()));
}

trusted_vault::VaultMember CreateVaultMember(
    const trusted_vault::SecureBoxPublicKey& public_key,
    std::vector<trusted_vault::MemberKeys> member_keys) {
  return trusted_vault::VaultMember(
      trusted_vault::SecureBoxPublicKey::CreateByImport(
          public_key.ExportToBytes()),
      std::move(member_keys));
}

class ICloudRecoveryUtilTest : public testing::Test {
 protected:
  std::unique_ptr<trusted_vault::ICloudRecoveryKey> CreateLocalKey() {
    base::test::TestFuture<std::unique_ptr<trusted_vault::ICloudRecoveryKey>>
        create_future;
    trusted_vault::ICloudRecoveryKey::Create(
        create_future.GetCallback(), trusted_vault::SecurityDomainId::kPasskeys,
        kICloudKeychainRecoveryKeyAccessGroup);
    return create_future.Take();
  }

  ICloudRecoveryResult Recover(
      base::span<const trusted_vault::VaultMember> security_domain_keys) {
    base::test::TestFuture<ICloudRecoveryResult> recover_future;
    RecoverSecurityDomainSecretFromICloudKeychain(security_domain_keys,
                                                  recover_future.GetCallback());
    return recover_future.Take();
  }

  crypto::apple::ScopedFakeKeychainV2 fake_keychain_{
      kICloudKeychainRecoveryKeyAccessGroup};
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ICloudRecoveryUtilTest, RecoverSingleMatchingKey) {
  std::unique_ptr<trusted_vault::ICloudRecoveryKey> local_key =
      CreateLocalKey();
  ASSERT_TRUE(local_key);
  const std::vector<uint8_t> kSecret = {1, 2, 3, 4, 5};
  constexpr int kVersion = 7;

  std::vector<trusted_vault::MemberKeys> member_keys;
  member_keys.push_back(CreateWrappedMemberKey(local_key->key()->public_key(),
                                               kSecret, kVersion));

  std::vector<trusted_vault::VaultMember> security_domain_keys;
  security_domain_keys.push_back(CreateVaultMember(
      local_key->key()->public_key(), std::move(member_keys)));

  ICloudRecoveryResult result = Recover(security_domain_keys);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->key, kSecret);
  EXPECT_EQ(result->version, kVersion);
}

TEST_F(ICloudRecoveryUtilTest, SelectsHighestVersionMemberKey) {
  std::unique_ptr<trusted_vault::ICloudRecoveryKey> local_key =
      CreateLocalKey();
  ASSERT_TRUE(local_key);
  const std::vector<uint8_t> kSecretV1 = {10, 11, 12};
  const std::vector<uint8_t> kSecretV5 = {50, 51, 52};
  const std::vector<uint8_t> kSecretV3 = {30, 31, 32};

  std::vector<trusted_vault::MemberKeys> member_keys;
  member_keys.push_back(CreateWrappedMemberKey(local_key->key()->public_key(),
                                               kSecretV1, /*version=*/1));
  member_keys.push_back(CreateWrappedMemberKey(local_key->key()->public_key(),
                                               kSecretV5, /*version=*/5));
  member_keys.push_back(CreateWrappedMemberKey(local_key->key()->public_key(),
                                               kSecretV3, /*version=*/3));

  std::vector<trusted_vault::VaultMember> security_domain_keys;
  security_domain_keys.push_back(CreateVaultMember(
      local_key->key()->public_key(), std::move(member_keys)));

  ICloudRecoveryResult result = Recover(security_domain_keys);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->key, kSecretV5);
  EXPECT_EQ(result->version, 5);
}

TEST_F(ICloudRecoveryUtilTest, MatchesAmongMultipleKeys) {
  auto remote_key_1 = trusted_vault::SecureBoxKeyPair::GenerateRandom();
  std::unique_ptr<trusted_vault::ICloudRecoveryKey> local_key_3 =
      CreateLocalKey();
  std::unique_ptr<trusted_vault::ICloudRecoveryKey> local_key_2 =
      CreateLocalKey();
  ASSERT_TRUE(local_key_3);
  ASSERT_TRUE(local_key_2);
  const std::vector<uint8_t> kSecret1 = {1, 1, 1};
  const std::vector<uint8_t> kSecret2 = {2, 2, 2};

  std::vector<trusted_vault::VaultMember> security_domain_keys;
  {
    std::vector<trusted_vault::MemberKeys> member_keys;
    member_keys.push_back(
        CreateWrappedMemberKey(remote_key_1->public_key(), kSecret1, 1));
    security_domain_keys.push_back(
        CreateVaultMember(remote_key_1->public_key(), std::move(member_keys)));
  }
  {
    std::vector<trusted_vault::MemberKeys> member_keys;
    member_keys.push_back(
        CreateWrappedMemberKey(local_key_2->key()->public_key(), kSecret2, 2));
    security_domain_keys.push_back(CreateVaultMember(
        local_key_2->key()->public_key(), std::move(member_keys)));
  }

  ICloudRecoveryResult result = Recover(security_domain_keys);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->key, kSecret2);
  EXPECT_EQ(result->version, 2);
}

TEST_F(ICloudRecoveryUtilTest, EmptySecurityDomainKeys) {
  ASSERT_TRUE(CreateLocalKey());
  std::vector<trusted_vault::VaultMember> security_domain_keys;

  EXPECT_EQ(Recover(security_domain_keys),
            base::unexpected(ICloudRecoveryError::kKeyNotFound));
}

TEST_F(ICloudRecoveryUtilTest, EmptyLocalKeys) {
  auto remote_key_pair = trusted_vault::SecureBoxKeyPair::GenerateRandom();
  std::vector<trusted_vault::MemberKeys> member_keys;
  member_keys.push_back(
      CreateWrappedMemberKey(remote_key_pair->public_key(), {1, 2, 3}, 1));

  std::vector<trusted_vault::VaultMember> security_domain_keys;
  security_domain_keys.push_back(
      CreateVaultMember(remote_key_pair->public_key(), std::move(member_keys)));

  EXPECT_EQ(Recover(security_domain_keys),
            base::unexpected(ICloudRecoveryError::kKeyNotFound));
}

TEST_F(ICloudRecoveryUtilTest, NoMatchingKey) {
  ASSERT_TRUE(CreateLocalKey());
  auto remote_key_pair = trusted_vault::SecureBoxKeyPair::GenerateRandom();

  std::vector<trusted_vault::MemberKeys> member_keys;
  member_keys.push_back(
      CreateWrappedMemberKey(remote_key_pair->public_key(), {1, 2, 3}, 1));

  std::vector<trusted_vault::VaultMember> security_domain_keys;
  security_domain_keys.push_back(
      CreateVaultMember(remote_key_pair->public_key(), std::move(member_keys)));

  EXPECT_EQ(Recover(security_domain_keys),
            base::unexpected(ICloudRecoveryError::kKeyNotFound));
}

TEST_F(ICloudRecoveryUtilTest, EmptyMemberKeysFailsDecryption) {
  std::unique_ptr<trusted_vault::ICloudRecoveryKey> local_key =
      CreateLocalKey();
  ASSERT_TRUE(local_key);

  std::vector<trusted_vault::VaultMember> security_domain_keys;
  security_domain_keys.push_back(
      CreateVaultMember(local_key->key()->public_key(), /*member_keys=*/{}));

  EXPECT_EQ(Recover(security_domain_keys),
            base::unexpected(ICloudRecoveryError::kDecryptionFailed));
}

TEST_F(ICloudRecoveryUtilTest, CorruptedWrappedKeyFailsDecryption) {
  std::unique_ptr<trusted_vault::ICloudRecoveryKey> local_key =
      CreateLocalKey();
  ASSERT_TRUE(local_key);

  std::vector<trusted_vault::MemberKeys> member_keys;
  member_keys.emplace_back(/*version=*/1,
                           /*wrapped_key=*/std::vector<uint8_t>{0xde, 0xad},
                           /*proof=*/std::vector<uint8_t>{0xbe, 0xef});

  std::vector<trusted_vault::VaultMember> security_domain_keys;
  security_domain_keys.push_back(CreateVaultMember(
      local_key->key()->public_key(), std::move(member_keys)));

  EXPECT_EQ(Recover(security_domain_keys),
            base::unexpected(ICloudRecoveryError::kDecryptionFailed));
}

}  // namespace
}  // namespace webauthn
