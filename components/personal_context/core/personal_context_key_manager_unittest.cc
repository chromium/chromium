// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_key_manager.h"

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/test/scoped_feature_list.h"
#include "components/personal_context/core/mock_personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/hybrid_encryption_key.pb.h"
#include "components/signin/public/base/tink_key.pb.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace personal_context {
namespace {

using ::testing::NiceMock;
using ::testing::Return;

class PersonalContextKeyManagerTest : public testing::Test {
 public:
  void SetUp() override {
    prefs::RegisterProfilePrefs(pref_service_.registry());
    ON_CALL(mock_eligibility_service_, IsInitialized())
        .WillByDefault(Return(true));
    ON_CALL(mock_eligibility_service_, IsEligibleForEncryption())
        .WillByDefault(Return(true));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kPersonalContextHandleEncryptedPayloads};
  TestingPrefServiceSimple pref_service_;
  syncer::FakeDeviceInfoSyncService fake_sync_service_;
  NiceMock<MockPersonalContextEligibilityService> mock_eligibility_service_;
};

TEST_F(PersonalContextKeyManagerTest, GeneratesAndPersistsKey) {
  std::vector<uint8_t> keyset_bytes1 =
      PersonalContextKeyManager::GetOrCreateLocalPublicKeyBytes(
          &pref_service_, &mock_eligibility_service_);
  EXPECT_FALSE(keyset_bytes1.empty());

  tink::Keyset keyset1;
  ASSERT_TRUE(
      keyset1.ParseFromArray(keyset_bytes1.data(), keyset_bytes1.size()));
  EXPECT_EQ(keyset1.primary_key_id(), 1u);
  ASSERT_EQ(keyset1.key_size(), 1);
  EXPECT_EQ(keyset1.key(0).key_data().type_url(),
            "type.googleapis.com/google.crypto.tink.HpkePublicKey");

  tink::HpkePublicKey hpke_public_key;
  ASSERT_TRUE(
      hpke_public_key.ParseFromString(keyset1.key(0).key_data().value()));
  EXPECT_EQ(hpke_public_key.version(), 0u);
  EXPECT_EQ(hpke_public_key.params().kem(), tink::HpkeKem::ML_KEM768);
  EXPECT_EQ(hpke_public_key.params().kdf(), tink::HpkeKdf::HKDF_SHA256);
  EXPECT_EQ(hpke_public_key.params().aead(), tink::HpkeAead::AES_128_GCM);
  EXPECT_EQ(hpke_public_key.public_key().size(), 1184u);

  // Subsequent call returns the same persisted key.
  std::vector<uint8_t> keyset_bytes2 =
      PersonalContextKeyManager::GetOrCreateLocalPublicKeyBytes(
          &pref_service_, &mock_eligibility_service_);
  EXPECT_EQ(keyset_bytes1, keyset_bytes2);
}

TEST_F(PersonalContextKeyManagerTest,
       GetOrCreateLocalPublicKeyBytes_NullEligibilityServiceReturnsEmpty) {
  EXPECT_TRUE(std::holds_alternative<
              syncer::DeviceInfo::PersonalContextInfo::NotEligible>(
      PersonalContextKeyManager::GetLocalPersonalContextInfo(
          &pref_service_, /*eligibility_service=*/nullptr)));
  EXPECT_TRUE(PersonalContextKeyManager::GetOrCreateLocalPublicKeyBytes(
                  &pref_service_, /*eligibility_service=*/nullptr)
                  .empty());
  EXPECT_TRUE(
      pref_service_.GetString(prefs::kPersonalContextPrivateKey).empty());
}

TEST_F(PersonalContextKeyManagerTest,
       GetOrCreateLocalPublicKeyBytes_NotInitializedReturnsEmpty) {
  ON_CALL(mock_eligibility_service_, IsInitialized())
      .WillByDefault(Return(false));
  EXPECT_TRUE(
      std::holds_alternative<syncer::DeviceInfo::PersonalContextInfo::NotReady>(
          PersonalContextKeyManager::GetLocalPersonalContextInfo(
              &pref_service_, &mock_eligibility_service_)));
  EXPECT_TRUE(PersonalContextKeyManager::GetOrCreateLocalPublicKeyBytes(
                  &pref_service_, &mock_eligibility_service_)
                  .empty());
  EXPECT_TRUE(
      pref_service_.GetString(prefs::kPersonalContextPrivateKey).empty());
}

TEST_F(PersonalContextKeyManagerTest,
       GetOrCreateLocalPublicKeyBytes_NotEligibleForEncryptionReturnsEmpty) {
  ON_CALL(mock_eligibility_service_, IsEligibleForEncryption())
      .WillByDefault(Return(false));
  EXPECT_TRUE(std::holds_alternative<
              syncer::DeviceInfo::PersonalContextInfo::NotEligible>(
      PersonalContextKeyManager::GetLocalPersonalContextInfo(
          &pref_service_, &mock_eligibility_service_)));
  EXPECT_TRUE(PersonalContextKeyManager::GetOrCreateLocalPublicKeyBytes(
                  &pref_service_, &mock_eligibility_service_)
                  .empty());
  EXPECT_TRUE(
      pref_service_.GetString(prefs::kPersonalContextPrivateKey).empty());
}

TEST_F(PersonalContextKeyManagerTest, EncryptsAndDecrypts) {
  PersonalContextKeyManager key_manager(&pref_service_, &fake_sync_service_,
                                        &mock_eligibility_service_);
  const std::string plaintext = "Personal context secret data";
  crypto::keypair::PublicKey recipient_pub = key_manager.GetPublicKey();

  std::optional<std::vector<uint8_t>> ciphertext =
      key_manager.Seal(recipient_pub, base::as_byte_span(plaintext));
  ASSERT_TRUE(ciphertext.has_value());

  std::optional<std::vector<uint8_t>> decrypted = key_manager.Open(*ciphertext);
  ASSERT_TRUE(decrypted.has_value());
  EXPECT_EQ(std::string(decrypted->begin(), decrypted->end()), plaintext);
}

TEST_F(PersonalContextKeyManagerTest, GeneratesKeyCallsRefreshLocalDeviceInfo) {
  ON_CALL(mock_eligibility_service_, IsInitialized())
      .WillByDefault(Return(false));
  PersonalContextKeyManager key_manager(&pref_service_, &fake_sync_service_,
                                        &mock_eligibility_service_);
  EXPECT_EQ(fake_sync_service_.RefreshLocalDeviceInfoCount(), 0);

  key_manager.GetOrCreatePrivateKey();
  EXPECT_EQ(fake_sync_service_.RefreshLocalDeviceInfoCount(), 1);

  // Loading an already generated key should not trigger an additional refresh.
  key_manager.GetOrCreatePrivateKey();
  EXPECT_EQ(fake_sync_service_.RefreshLocalDeviceInfoCount(), 1);
}

TEST_F(PersonalContextKeyManagerTest,
       ConstructorGeneratesKeyWhenInitializedAndEligible) {
  syncer::FakeDeviceInfoSyncService fake_sync_service;
  PersonalContextKeyManager key_manager(&pref_service_, &fake_sync_service,
                                        &mock_eligibility_service_);
  EXPECT_FALSE(
      pref_service_.GetString(prefs::kPersonalContextPrivateKey).empty());
  EXPECT_EQ(fake_sync_service.RefreshLocalDeviceInfoCount(), 1);
}

TEST_F(PersonalContextKeyManagerTest,
       OnEncryptionEligibilityChanged_GeneratesKeyAndRefreshesDeviceInfo) {
  ON_CALL(mock_eligibility_service_, IsInitialized())
      .WillByDefault(Return(false));
  ON_CALL(mock_eligibility_service_, IsEligibleForEncryption())
      .WillByDefault(Return(false));

  syncer::FakeDeviceInfoSyncService fake_sync_service;
  PersonalContextKeyManager key_manager(&pref_service_, &fake_sync_service,
                                        &mock_eligibility_service_);
  EXPECT_TRUE(
      pref_service_.GetString(prefs::kPersonalContextPrivateKey).empty());
  EXPECT_EQ(fake_sync_service.RefreshLocalDeviceInfoCount(), 0);

  ON_CALL(mock_eligibility_service_, IsInitialized())
      .WillByDefault(Return(true));
  ON_CALL(mock_eligibility_service_, IsEligibleForEncryption())
      .WillByDefault(Return(true));
  key_manager.OnEncryptionEligibilityChanged(true);

  EXPECT_FALSE(
      pref_service_.GetString(prefs::kPersonalContextPrivateKey).empty());
  EXPECT_EQ(fake_sync_service.RefreshLocalDeviceInfoCount(), 1);
}

TEST_F(
    PersonalContextKeyManagerTest,
    OnEncryptionEligibilityChanged_BecomingIneligiblePreservesKeyAndRefreshesDeviceInfo) {
  syncer::FakeDeviceInfoSyncService fake_sync_service;
  PersonalContextKeyManager key_manager(&pref_service_, &fake_sync_service,
                                        &mock_eligibility_service_);
  const std::string initial_key =
      pref_service_.GetString(prefs::kPersonalContextPrivateKey);
  ASSERT_FALSE(initial_key.empty());
  EXPECT_EQ(fake_sync_service.RefreshLocalDeviceInfoCount(), 1);

  ON_CALL(mock_eligibility_service_, IsEligibleForEncryption())
      .WillByDefault(Return(false));
  ON_CALL(mock_eligibility_service_, GetNonEligibilityReason())
      .WillByDefault(Return(PersonalContextNonEligibilityReason::
                                kNotPhotosAndWorkspaceAvailable));
  key_manager.OnEncryptionEligibilityChanged(false);

  // DeviceInfo is refreshed so the public key stops being shared, while the
  // private key in prefs is preserved.
  EXPECT_EQ(fake_sync_service.RefreshLocalDeviceInfoCount(), 2);
  EXPECT_EQ(pref_service_.GetString(prefs::kPersonalContextPrivateKey),
            initial_key);
  EXPECT_TRUE(PersonalContextKeyManager::GetOrCreateLocalPublicKeyBytes(
                  &pref_service_, &mock_eligibility_service_)
                  .empty());
}

TEST_F(
    PersonalContextKeyManagerTest,
    OnEncryptionEligibilityChanged_IneligibleWithoutExistingKeyDoesNotRefreshDeviceInfo) {
  ON_CALL(mock_eligibility_service_, IsInitialized())
      .WillByDefault(Return(false));
  ON_CALL(mock_eligibility_service_, IsEligibleForEncryption())
      .WillByDefault(Return(false));
  ON_CALL(mock_eligibility_service_, GetNonEligibilityReason())
      .WillByDefault(Return(PersonalContextNonEligibilityReason::
                                kNotPhotosAndWorkspaceAvailable));

  syncer::FakeDeviceInfoSyncService fake_sync_service;
  PersonalContextKeyManager key_manager(&pref_service_, &fake_sync_service,
                                        &mock_eligibility_service_);
  ASSERT_TRUE(
      pref_service_.GetString(prefs::kPersonalContextPrivateKey).empty());

  ON_CALL(mock_eligibility_service_, IsInitialized())
      .WillByDefault(Return(true));
  key_manager.OnEncryptionEligibilityChanged(false);

  EXPECT_EQ(fake_sync_service.RefreshLocalDeviceInfoCount(), 0);
}

TEST_F(
    PersonalContextKeyManagerTest,
    OnEncryptionEligibilityChanged_BecomingEligibleAgainReusesKeyAndRefreshesDeviceInfo) {
  syncer::FakeDeviceInfoSyncService fake_sync_service;
  PersonalContextKeyManager key_manager(&pref_service_, &fake_sync_service,
                                        &mock_eligibility_service_);
  const std::string initial_private_key =
      pref_service_.GetString(prefs::kPersonalContextPrivateKey);
  ASSERT_FALSE(initial_private_key.empty());
  const std::vector<uint8_t> initial_public_key_bytes =
      PersonalContextKeyManager::GetOrCreateLocalPublicKeyBytes(
          &pref_service_, &mock_eligibility_service_);
  ASSERT_FALSE(initial_public_key_bytes.empty());
  EXPECT_EQ(fake_sync_service.RefreshLocalDeviceInfoCount(), 1);

  // Become ineligible.
  ON_CALL(mock_eligibility_service_, IsEligibleForEncryption())
      .WillByDefault(Return(false));
  ON_CALL(mock_eligibility_service_, GetNonEligibilityReason())
      .WillByDefault(Return(PersonalContextNonEligibilityReason::
                                kNotPhotosAndWorkspaceAvailable));
  key_manager.OnEncryptionEligibilityChanged(false);

  EXPECT_EQ(fake_sync_service.RefreshLocalDeviceInfoCount(), 2);
  EXPECT_EQ(pref_service_.GetString(prefs::kPersonalContextPrivateKey),
            initial_private_key);
  EXPECT_TRUE(PersonalContextKeyManager::GetOrCreateLocalPublicKeyBytes(
                  &pref_service_, &mock_eligibility_service_)
                  .empty());

  // Become eligible again.
  ON_CALL(mock_eligibility_service_, IsEligibleForEncryption())
      .WillByDefault(Return(true));
  ON_CALL(mock_eligibility_service_, GetNonEligibilityReason())
      .WillByDefault(Return(std::nullopt));
  key_manager.OnEncryptionEligibilityChanged(true);

  EXPECT_EQ(fake_sync_service.RefreshLocalDeviceInfoCount(), 3);
  EXPECT_EQ(pref_service_.GetString(prefs::kPersonalContextPrivateKey),
            initial_private_key);
  EXPECT_EQ(PersonalContextKeyManager::GetOrCreateLocalPublicKeyBytes(
                &pref_service_, &mock_eligibility_service_),
            initial_public_key_bytes);
}

}  // namespace
}  // namespace personal_context
