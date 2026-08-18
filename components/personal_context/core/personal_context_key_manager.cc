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
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/tink_key.pb.h"

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
  // TODO(b/544747336): Call DeviceInfoSyncService::RefreshLocalDeviceInfo()
  // when a new key is generated so that the public key is immediately committed
  // without delay.
  // TODO(b/544747336): Add an integration test in
  // SingleClientDeviceInfoSyncTest to verify public key upload to the sync
  // server.
  return key;
}

crypto::keypair::PrivateKey LoadOrGeneratePrivateKey(PrefService* prefs) {
  CHECK(prefs);
  std::string base64_key =
      prefs->GetString(prefs::kPersonalContextPrivateKey);
  if (base64_key.empty()) {
    return GenerateAndStorePrivateKey(prefs);
  }

  std::optional<std::vector<uint8_t>> decoded = base::Base64Decode(base64_key);
  if (!decoded || decoded->size() != 64) {
    return GenerateAndStorePrivateKey(prefs);
  }

  return crypto::keypair::PrivateKey::FromMlkem768PrivateKey(
      base::as_byte_span(*decoded).first<64>());
}

}  // namespace

PersonalContextKeyManager::PersonalContextKeyManager(PrefService* prefs)
    : prefs_(prefs) {
  CHECK(prefs_);
}

PersonalContextKeyManager::~PersonalContextKeyManager() = default;

std::vector<uint8_t>
PersonalContextKeyManager::GetOrCreateLocalPublicKeyBytes(PrefService* prefs) {
  CHECK(prefs);
  crypto::keypair::PrivateKey priv = LoadOrGeneratePrivateKey(prefs);
  std::array<uint8_t, 1184> pub_bytes = priv.ToMlkem768PublicKey();

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
  key_data->set_value(base::as_string_view(pub_bytes));
  key_data->set_key_material_type(tink::KeyData::ASYMMETRIC_PUBLIC);

  std::string serialized = keyset.SerializeAsString();
  return base::ToVector(base::as_byte_span(serialized));
}

crypto::keypair::PrivateKey PersonalContextKeyManager::GetOrCreatePrivateKey() {
  // TODO(b/544747336): Clean up or rotate keys on profile signout.
  if (!private_key_) {
    private_key_ = LoadOrGeneratePrivateKey(prefs_);
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
  return crypto::hpke::Open(kPersonalContextHpkeParams, GetOrCreatePrivateKey(),
                            encrypted_data, info, ad);
}

}  // namespace personal_context

