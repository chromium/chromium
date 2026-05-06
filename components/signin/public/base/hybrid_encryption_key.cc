// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/hybrid_encryption_key.h"

#include "base/check_is_test.h"
#include "base/containers/to_vector.h"
#include "components/signin/public/base/hybrid_encryption_key.pb.h"
#include "components/signin/public/base/tink_key.pb.h"
#include "crypto/hpke.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace {

constexpr crypto::hpke::HpkeParams kHpkeParams = {
    .kem = crypto::hpke::KemType::kX25519HkdfSha256,
    .kdf = crypto::hpke::KdfType::kHkdfSha256,
    .aead = crypto::hpke::AeadType::kAes128Gcm};

// These must match the params given above.
constexpr auto kTinkKem = tink::HpkeKem::DHKEM_X25519_HKDF_SHA256;
constexpr auto kTinkHkdf = tink::HpkeKdf::HKDF_SHA256;
constexpr auto kTinkAead = tink::HpkeAead::AES_128_GCM;

}  // namespace

HybridEncryptionKey::HybridEncryptionKey()
    : private_key_(crypto::keypair::PrivateKey::GenerateX25519()) {}

HybridEncryptionKey::~HybridEncryptionKey() = default;

HybridEncryptionKey::HybridEncryptionKey(HybridEncryptionKey&& other) noexcept =
    default;
HybridEncryptionKey& HybridEncryptionKey::operator=(
    HybridEncryptionKey&& other) noexcept = default;

std::optional<std::vector<uint8_t>> HybridEncryptionKey::Decrypt(
    base::span<const uint8_t> encrypted_data) const {
  return crypto::hpke::Open(kHpkeParams, private_key_, encrypted_data,
                            /*info=*/{}, /*ad=*/{});
}

std::string HybridEncryptionKey::ExportPublicKey() const {
  tink::HpkePublicKey hpke_public_key;
  hpke_public_key.set_version(0);
  tink::HpkeParams* params = hpke_public_key.mutable_params();
  params->set_kem(kTinkKem);
  params->set_kdf(kTinkHkdf);
  params->set_aead(kTinkAead);
  std::vector<uint8_t> public_key = GetPublicKey();
  hpke_public_key.set_public_key(
      std::string(public_key.begin(), public_key.end()));

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
  return keyset.SerializeAsString();
}

std::vector<uint8_t> HybridEncryptionKey::EncryptForTesting(
    base::span<const uint8_t> plaintext) const {
  crypto::keypair::PublicKey pub =
      crypto::keypair::PublicKey::FromPrivateKey(private_key_);
  auto result =
      crypto::hpke::Seal(kHpkeParams, pub, plaintext, /*info=*/{}, /*ad=*/{});
  return result.value_or({});
}

HybridEncryptionKey::HybridEncryptionKey(base::span<const uint8_t> private_key)
    : private_key_(crypto::keypair::PrivateKey::FromX25519PrivateKey(
          base::span<const uint8_t, X25519_PRIVATE_KEY_LEN>(private_key))) {
  CHECK_IS_TEST();
}

std::vector<uint8_t> HybridEncryptionKey::GetPublicKey() const {
  auto pub = crypto::keypair::PublicKey::FromPrivateKey(private_key_)
                 .ToX25519PublicKey();
  return base::ToVector(pub);
}
