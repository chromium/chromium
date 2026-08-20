// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_KEY_MANAGER_H_
#define COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_KEY_MANAGER_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "crypto/hpke.h"
#include "crypto/keypair.h"

class PrefService;

namespace syncer {
class DeviceInfoSyncService;
}  // namespace syncer

namespace personal_context {

// Default HPKE parameters for Personal Context:
// ML-KEM-768, HKDF-SHA256, AES-128-GCM.
inline constexpr crypto::hpke::HpkeParams kPersonalContextHpkeParams{
    .kem = crypto::hpke::KemType::kMlkem768,
    .kdf = crypto::hpke::KdfType::kHkdfSha256,
    .aead = crypto::hpke::AeadType::kAes128Gcm,
};

// Manages the local device HPKE key pair for Personal Context payload
// encryption and decryption.
class PersonalContextKeyManager {
 public:
  PersonalContextKeyManager(
      PrefService* prefs,
      syncer::DeviceInfoSyncService* device_info_sync_service);
  PersonalContextKeyManager(const PersonalContextKeyManager&) = delete;
  PersonalContextKeyManager& operator=(const PersonalContextKeyManager&) = delete;
  ~PersonalContextKeyManager();

  // Returns the local serialized proto bytes of tink.Keyset containing this
  // device's public key.
  // Generates a new key pair in `prefs` if none exists yet.
  static std::vector<uint8_t> GetOrCreateLocalPublicKeyBytes(
      PrefService* prefs);

  // Returns the PrivateKey instance, creating and persisting it to `prefs` if
  // needed.
  crypto::keypair::PrivateKey GetOrCreatePrivateKey();

  // Returns the PublicKey instance corresponding to this device's private key.
  crypto::keypair::PublicKey GetPublicKey();

  // Encrypts `plaintext` using HPKE Base Mode for `recipient_public_key`.
  // Returns encapsulated shared secret followed by ciphertext.
  std::optional<std::vector<uint8_t>> Seal(
      const crypto::keypair::PublicKey& recipient_public_key,
      base::span<const uint8_t> plaintext,
      base::span<const uint8_t> info = {},
      base::span<const uint8_t> ad = {});

  // Decrypts `encrypted_data` (encapsulated shared secret + ciphertext) using
  // this device's private key.
  std::optional<std::vector<uint8_t>> Open(
      base::span<const uint8_t> encrypted_data,
      base::span<const uint8_t> info = {},
      base::span<const uint8_t> ad = {});

 private:
  const raw_ptr<PrefService> prefs_;
  const raw_ptr<syncer::DeviceInfoSyncService> device_info_sync_service_;
  std::optional<crypto::keypair::PrivateKey> private_key_;
};

}  // namespace personal_context

#endif  // COMPONENTS_PERSONAL_CONTEXT_CORE_PERSONAL_CONTEXT_KEY_MANAGER_H_

