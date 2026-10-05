// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_key_manager.h"

#include <array>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/strings/string_view_util.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/hybrid_encryption_key.pb.h"
#include "components/signin/public/base/tink_key.pb.h"
#include "components/sync_device_info/device_info_sync_service.h"

namespace personal_context {

namespace {

crypto::keypair::PrivateKey GenerateAndStorePrivateKey(PrefService* prefs) {
  CHECK(prefs);
  crypto::keypair::PrivateKey key =
      crypto::keypair::PrivateKey::GenerateMlkem768();
  std::array<uint8_t, 64> priv_bytes = key.ToMlkem768PrivateKey();
  // TODO(b/544747336): Consider encrypting the stored private key via
  // OSCrypt / OSCrypt Async before saving to prefs.
  prefs->SetString(prefs::kPersonalContextPrivateKey,
                   base::Base64Encode(priv_bytes));
  return key;
}

crypto::keypair::PrivateKey LoadOrGeneratePrivateKey(
    PrefService* prefs,
    bool* key_generated = nullptr) {
  CHECK(prefs);
  std::string base64_key =
      prefs->GetString(prefs::kPersonalContextPrivateKey);
  if (base64_key.empty()) {
    if (key_generated) {
      *key_generated = true;
    }
    return GenerateAndStorePrivateKey(prefs);
  }

  std::optional<std::vector<uint8_t>> decoded = base::Base64Decode(base64_key);
  if (!decoded || decoded->size() != 64) {
    if (key_generated) {
      *key_generated = true;
    }
    return GenerateAndStorePrivateKey(prefs);
  }

  if (key_generated) {
    *key_generated = false;
  }
  return crypto::keypair::PrivateKey::FromMlkem768PrivateKey(
      base::as_byte_span(*decoded).first<64>());
}

}  // namespace

PersonalContextKeyManager::PersonalContextKeyManager(
    PrefService* prefs,
    syncer::DeviceInfoSyncService* device_info_sync_service,
    PersonalContextEligibilityService* eligibility_service)
    : prefs_(prefs),
      device_info_sync_service_(device_info_sync_service),
      eligibility_service_(eligibility_service) {
  CHECK(prefs_);
  CHECK(device_info_sync_service_);
  CHECK(eligibility_service_);
  eligibility_observation_.Observe(eligibility_service_);
  if (eligibility_service_->IsInitialized() &&
      eligibility_service_->IsEligibleForEncryption()) {
    GetOrCreatePrivateKey();
  }
}

PersonalContextKeyManager::~PersonalContextKeyManager() = default;

// static
syncer::DeviceInfo::PersonalContextInfo::StatusOrInfo
PersonalContextKeyManager::GetLocalPersonalContextInfo(
    PrefService* prefs,
    const PersonalContextEligibilityService* eligibility_service) {
  if (!base::FeatureList::IsEnabled(
          features::kPersonalContextHandleEncryptedPayloads)) {
    return syncer::DeviceInfo::PersonalContextInfo::NotEligible();
  }
  CHECK(prefs);
  if (!eligibility_service) {
    return syncer::DeviceInfo::PersonalContextInfo::NotEligible();
  }
  if (!eligibility_service->IsInitialized()) {
    return syncer::DeviceInfo::PersonalContextInfo::NotReady();
  }
  if (!eligibility_service->IsEligibleForEncryption()) {
    return syncer::DeviceInfo::PersonalContextInfo::NotEligible();
  }
  crypto::keypair::PrivateKey priv = LoadOrGeneratePrivateKey(prefs);
  std::array<uint8_t, 1184> pub_bytes = priv.ToMlkem768PublicKey();

  tink::HpkePublicKey hpke_public_key;
  hpke_public_key.set_version(0);
  tink::HpkeParams* params = hpke_public_key.mutable_params();
  params->set_kem(tink::HpkeKem::ML_KEM768);
  params->set_kdf(tink::HpkeKdf::HKDF_SHA256);
  params->set_aead(tink::HpkeAead::AES_128_GCM);
  hpke_public_key.set_public_key(base::as_string_view(pub_bytes));

  const uint32_t key_id = 1;
  tink::Keyset keyset;
  keyset.set_primary_key_id(key_id);
  tink::Keyset_Key* keyset_key = keyset.add_key();
  keyset_key->set_status(tink::KeyStatusType::ENABLED);
  keyset_key->set_output_prefix_type(tink::OutputPrefixType::RAW);
  keyset_key->set_key_id(key_id);
  tink::KeyData* key_data = keyset_key->mutable_key_data();
  key_data->set_type_url(
      "type.googleapis.com/google.crypto.tink.HpkePublicKey");
  key_data->set_value(hpke_public_key.SerializeAsString());
  key_data->set_key_material_type(tink::KeyData::ASYMMETRIC_PUBLIC);

  std::string serialized = keyset.SerializeAsString();
  return syncer::DeviceInfo::PersonalContextInfo{
      .serialized_tink_keyset = base::ToVector(base::as_byte_span(serialized))};
}

// static
std::vector<uint8_t> PersonalContextKeyManager::GetOrCreateLocalPublicKeyBytes(
    PrefService* prefs,
    const PersonalContextEligibilityService* eligibility_service) {
  syncer::DeviceInfo::PersonalContextInfo::StatusOrInfo status_or_info =
      GetLocalPersonalContextInfo(prefs, eligibility_service);
  if (auto* info = std::get_if<syncer::DeviceInfo::PersonalContextInfo>(
          &status_or_info)) {
    return std::move(info->serialized_tink_keyset);
  }
  return {};
}

crypto::keypair::PrivateKey PersonalContextKeyManager::GetOrCreatePrivateKey() {
  // TODO(b/544747336): Clean up or rotate keys on profile signout.
  if (!private_key_) {
    bool key_generated = false;
    private_key_ = LoadOrGeneratePrivateKey(prefs_, &key_generated);
    if (key_generated) {
      device_info_sync_service_->RefreshLocalDeviceInfo();
    }
  }
  return *private_key_;
}

crypto::keypair::PublicKey PersonalContextKeyManager::GetPublicKey() {
  return crypto::keypair::PublicKey::FromPrivateKey(GetOrCreatePrivateKey());
}

std::optional<std::vector<uint8_t>> PersonalContextKeyManager::Seal(
    const crypto::keypair::PublicKey& recipient_public_key,
    base::span<const uint8_t> plaintext,
    base::span<const uint8_t> info,
    base::span<const uint8_t> ad) {
  return crypto::hpke::Seal(kPersonalContextHpkeParams, recipient_public_key,
                            plaintext, info, ad);
}

std::optional<std::vector<uint8_t>> PersonalContextKeyManager::Open(
    base::span<const uint8_t> encrypted_data,
    base::span<const uint8_t> info,
    base::span<const uint8_t> ad) {
  // TODO(b/544747336): return nullopt if a key isn't available for decryption.
  return crypto::hpke::Open(kPersonalContextHpkeParams, GetOrCreatePrivateKey(),
                            encrypted_data, info, ad);
}

void PersonalContextKeyManager::OnEncryptionEligibilityChanged(
    bool is_eligible) {
  // Note: `OnEncryptionEligibilityChanged()` is also called on every browser
  // startup once the eligibility service finishes initializing. Calling
  // `RefreshLocalDeviceInfo()` is safe because it will not trigger a
  // DeviceInfo upload to the Sync server if the local DeviceInfo value has not
  // changed.
  if (is_eligible) {
    // Ensure a local key pair exists in prefs and refresh DeviceInfo so the
    // public key is shared with the Sync server (including when re-enabling
    // eligibility with an already-persisted key).
    if (!private_key_) {
      private_key_ = LoadOrGeneratePrivateKey(prefs_);
    }

    device_info_sync_service_->RefreshLocalDeviceInfo();

    return;
  }

  // When becoming ineligible, preserve the local private key in prefs so the
  // device does not need to rotate keys if it becomes eligible again. If a key
  // was previously generated, refresh DeviceInfo so the public key is no
  // longer shared with the Sync server.
  if (!prefs_->GetString(prefs::kPersonalContextPrivateKey).empty()) {
    device_info_sync_service_->RefreshLocalDeviceInfo();
  }
}

}  // namespace personal_context
