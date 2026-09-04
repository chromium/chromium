// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_key_manager.h"

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/hybrid_encryption_key.pb.h"
#include "components/signin/public/base/tink_key.pb.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace personal_context {
namespace {

class PersonalContextKeyManagerTest : public testing::Test {
 public:
  void SetUp() override {
    prefs::RegisterProfilePrefs(pref_service_.registry());
    key_manager_ = std::make_unique<PersonalContextKeyManager>(
        &pref_service_, /*device_info_sync_service=*/nullptr);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<PersonalContextKeyManager> key_manager_;
};

TEST_F(PersonalContextKeyManagerTest, GeneratesAndPersistsKey) {
  std::vector<uint8_t> keyset_bytes1 =
      PersonalContextKeyManager::GetOrCreateLocalPublicKeyBytes(&pref_service_);
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
      PersonalContextKeyManager::GetOrCreateLocalPublicKeyBytes(&pref_service_);
  EXPECT_EQ(keyset_bytes1, keyset_bytes2);
}

TEST_F(PersonalContextKeyManagerTest, EncryptsAndDecrypts) {
  const std::string plaintext = "Personal context secret data";
  crypto::keypair::PublicKey recipient_pub = key_manager_->GetPublicKey();

  std::optional<std::vector<uint8_t>> ciphertext = key_manager_->Seal(
      recipient_pub, base::as_byte_span(plaintext));
  ASSERT_TRUE(ciphertext.has_value());

  std::optional<std::vector<uint8_t>> decrypted =
      key_manager_->Open(*ciphertext);
  ASSERT_TRUE(decrypted.has_value());
  EXPECT_EQ(std::string(decrypted->begin(), decrypted->end()), plaintext);
}

TEST_F(PersonalContextKeyManagerTest, GeneratesKeyCallsRefreshLocalDeviceInfo) {
  syncer::FakeDeviceInfoSyncService fake_sync_service;
  PersonalContextKeyManager key_manager(&pref_service_, &fake_sync_service);
  EXPECT_EQ(fake_sync_service.RefreshLocalDeviceInfoCount(), 0);

  key_manager.GetOrCreatePrivateKey();
  EXPECT_EQ(fake_sync_service.RefreshLocalDeviceInfoCount(), 1);

  // Loading an already generated key should not trigger an additional refresh.
  key_manager.GetOrCreatePrivateKey();
  EXPECT_EQ(fake_sync_service.RefreshLocalDeviceInfoCount(), 1);
}

}  // namespace
}  // namespace personal_context

