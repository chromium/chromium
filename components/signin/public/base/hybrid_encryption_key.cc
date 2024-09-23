// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/hybrid_encryption_key.h"
#include "components/signin/public/base/hybrid_encryption_key.pb.h"
#include "components/signin/public/base/tink_key.pb.h"

namespace {
const size_t kEncapsulatedKeySize = 32;

const EVP_HPKE_KEM* GetKem() {
  return EVP_hpke_x25519_hkdf_sha256();
}

tink::HpkeKem GetKemType(const EVP_HPKE_KEM* kem) {
  if (EVP_HPKE_KEM_id(kem) == EVP_HPKE_DHKEM_X25519_HKDF_SHA256) {
    return tink::HpkeKem::DHKEM_X25519_HKDF_SHA256;
  } else {
    return tink::HpkeKem::KEM_UNKNOWN;
  }
}

const EVP_HPKE_KDF* GetKdf() {
  return EVP_hpke_hkdf_sha256();
}

tink::HpkeKdf GetKdfType(const EVP_HPKE_KDF* kdf) {
  if (EVP_HPKE_KDF_id(kdf) == EVP_HPKE_HKDF_SHA256) {
    return tink::HpkeKdf::HKDF_SHA256;
  } else {
    return tink::HpkeKdf::KDF_UNKNOWN;
  }
}

const EVP_HPKE_AEAD* GetAead() {
  return EVP_hpke_aes_128_gcm();
}

tink::HpkeAead GetAeadType(const EVP_HPKE_AEAD* aead) {
  if (EVP_HPKE_AEAD_id(aead) == EVP_HPKE_AES_128_GCM) {
    return tink::HpkeAead::AES_128_GCM;
  } else {
    return tink::HpkeAead::AEAD_UNKNOWN;
  }
}
}  // namespace

HybridEncryptionKey::HybridEncryptionKey() {
  // `EVP_HPKE_KEY_generate` should never fail for the provided KEM (x25519).
  CHECK(EVP_HPKE_KEY_generate(key_.get(), GetKem()));
}

HybridEncryptionKey::~HybridEncryptionKey() = default;

HybridEncryptionKey::HybridEncryptionKey(HybridEncryptionKey&& other) noexcept =
    default;
HybridEncryptionKey& HybridEncryptionKey::operator=(
    HybridEncryptionKey&& other) noexcept = default;

std::optional<std::vector<uint8_t>> HybridEncryptionKey::Decrypt(
    base::span<const uint8_t> encrypted_data) const {
  if (encrypted_data.size() < kEncapsulatedKeySize) {
    return std::nullopt;
  }
  base::span<const uint8_t> encapsulated_key =
      encrypted_data.subspan(0, kEncapsulatedKeySize);

  bssl::ScopedEVP_HPKE_CTX recipient_context;
  if (!EVP_HPKE_CTX_setup_recipient(
          recipient_context.get(), key_.get(), GetKdf(), GetAead(),
          encapsulated_key.data(), encapsulated_key.size(), nullptr, 0)) {
    return std::nullopt;
  }

  base::span<const uint8_t> ciphertext =
      encrypted_data.subspan(kEncapsulatedKeySize);
  std::vector<uint8_t> plaintext(ciphertext.size());
  size_t plaintext_len;

  if (!EVP_HPKE_CTX_open(recipient_context.get(), plaintext.data(),
                         &plaintext_len, plaintext.size(), ciphertext.data(),
                         ciphertext.size(), nullptr, 0)) {
    return std::nullopt;
  }
  plaintext.resize(plaintext_len);
  return plaintext;
}

std::string HybridEncryptionKey::ExportPublicKey() const {
  tink::HpkePublicKey hpke_public_key;
  hpke_public_key.set_version(0);
  tink::HpkeParams* params = hpke_public_key.mutable_params();
  params->set_kem(GetKemType(GetKem()));
  params->set_kdf(GetKdfType(GetKdf()));
  params->set_aead(GetAeadType(GetAead()));
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
  std::vector<uint8_t> public_key = GetPublicKey();
  // This vector will hold the encapsulated key followed by the ciphertext.
  std::vector<uint8_t> encrypted_data(kEncapsulatedKeySize);
  size_t encapsulated_key_len;

  bssl::ScopedEVP_HPKE_CTX sender_context;
  if (!EVP_HPKE_CTX_setup_sender(
          sender_context.get(), encrypted_data.data(), &encapsulated_key_len,
          encrypted_data.size(), GetKem(), GetKdf(), GetAead(),
          public_key.data(), public_key.size(), nullptr, 0)) {
    return {};
  }

  encrypted_data.resize(kEncapsulatedKeySize + plaintext.size() +
                        EVP_HPKE_CTX_max_overhead(sender_context.get()));

  base::span<uint8_t> ciphertext =
      base::make_span(encrypted_data).subspan(kEncapsulatedKeySize);
  size_t ciphertext_len;

  if (!EVP_HPKE_CTX_seal(sender_context.get(), ciphertext.data(),
                         &ciphertext_len, ciphertext.size(), plaintext.data(),
                         plaintext.size(), nullptr, 0)) {
    return {};
  }
  // Reset `ciphertext` before changing the underlying container size as a
  // safety precaution.
  ciphertext = base::span<uint8_t>();
  encrypted_data.resize(kEncapsulatedKeySize + ciphertext_len);
  return encrypted_data;
}

HybridEncryptionKey::HybridEncryptionKey(
    base::span<const uint8_t> private_key) {
  CHECK(EVP_HPKE_KEY_init(key_.get(), GetKem(), private_key.data(),
                          private_key.size()));
}

std::vector<uint8_t> HybridEncryptionKey::GetPublicKey() const {
  std::vector<uint8_t> public_key(EVP_HPKE_MAX_PUBLIC_KEY_LENGTH);
  size_t public_key_len;
  EVP_HPKE_KEY_public_key(key_.get(), public_key.data(), &public_key_len,
                          public_key.size());
  public_key.resize(public_key_len);
  return public_key;
}
