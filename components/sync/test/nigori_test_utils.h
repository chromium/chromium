// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_NIGORI_TEST_UTILS_H_
#define COMPONENTS_SYNC_TEST_NIGORI_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_key.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/nigori/cross_user_sharing_keys.h"

namespace sync_pb {

class BookmarkSpecifics;
class NigoriSpecifics;
class EntitySpecifics;

}  // namespace sync_pb

namespace syncer {

class Cryptographer;

struct KeyParamsForTesting {
  KeyDerivationParams derivation_params;
  std::string password;
};

// Creates KeyParamsForTesting, where |derivation_params| is Pbkdf2
// KeyDerivationParams and |password| is base64 encoded |raw_key|.
KeyParamsForTesting KeystoreKeyParamsForTesting(
    const std::vector<uint8_t>& raw_key);

// Creates KeyParamsForTesting, where |derivation_params| is Pbkdf2
// KeyDerivationParams and |password| is base64 encoded |raw_key|.
KeyParamsForTesting TrustedVaultKeyParamsForTesting(
    const std::vector<uint8_t>& raw_key);

// Creates KeyParamsForTesting, where |derivation_params| is Pbdf2
// KeyDerivationParams and |password| is |passphrase|.
KeyParamsForTesting Pbkdf2PassphraseKeyParamsForTesting(
    const std::string& passphrase);

// Creates KeyParamsForTesting, where |derivation_params| is Scrypt
// KeyDerivationParams with constant salt and |password| is |passphrase|.
KeyParamsForTesting ScryptPassphraseKeyParamsForTesting(
    const std::string& passphrase);

// Builds NigoriSpecifics with following fields:
// 1. encryption_keybag contains all keys derived from |keybag_keys_params|
// and encrypted with a key derived from |keystore_decryptor_params|.
// 2. keystore_decryptor_token contains the key derived from
// |keystore_decryptor_params| and encrypted with a key derived from
// |keystore_key_params|.
// 3. passphrase_type is KEYSTORE_PASSHPRASE.
// 4. Other fields are default.
// |keybag_keys_params| must be non-empty.
// |cross_user_sharing_keys| can be empty and contains cross user sharing keys.
sync_pb::NigoriSpecifics BuildKeystoreNigoriSpecifics(
    const std::vector<KeyParamsForTesting>& keybag_keys_params,
    const KeyParamsForTesting& keystore_decryptor_params,
    const KeyParamsForTesting& keystore_key_params,
    const CrossUserSharingKeys& cross_user_sharing_keys =
        CrossUserSharingKeys::CreateEmpty());

// Builds NigoriSpecifics with following fields:
// 1. encryption_keybag contains all keys derived from |keybag_keys_params|
// and encrypted with a key derived from |keystore_decryptor_params|.
// 2. keystore_decryptor_token contains the key derived from
// |keystore_decryptor_params| and encrypted with a key derived from
// |keystore_key_params|.
// 3. passphrase_type is KEYSTORE_PASSHPRASE.
// 4. Other fields are default.
// |keybag_keys_params| must be non-empty.
// |cross_user_sharing_keys| can be empty and contains cross user sharing keys.
// |cross_user_sharing_public_key| is the public to register.
// |cross_user_sharing_public_key_version| is the associated version of the
// public key to register.
sync_pb::NigoriSpecifics BuildKeystoreNigoriSpecificsWithCrossUserSharingKeys(
    const std::vector<KeyParamsForTesting>& keybag_keys_params,
    const KeyParamsForTesting& keystore_decryptor_params,
    const KeyParamsForTesting& keystore_key_params,
    const CrossUserSharingKeys& cross_user_sharing_keys,
    const CrossUserSharingPublicKey& cross_user_sharing_public_key,
    const uint32_t cross_user_sharing_public_key_version);

// Builds NigoriSpecifics with following fields:
// 1. encryption_keybag contains keys derived from |trusted_vault_keys| and
// encrypted with key derived from last of them.
// 2. passphrase_type is TRUSTED_VAULT_PASSPHRASE.
// 3. keybag_is_frozen set to true.
//
// |migration_time| allows the caller to specify a trusted vault migration time
// as represented in |TrustedVaultDebugInfo|.
sync_pb::NigoriSpecifics BuildTrustedVaultNigoriSpecifics(
    const std::vector<std::vector<uint8_t>>& trusted_vault_keys,
    base::Time migration_time = base::Time::UnixEpoch());

// Creates a NigoriSpecifics that describes encryption using a custom
// passphrase with the given |passphrase_key_params|. If |old_key_params| is
// presented, |encryption_keybag| will also contain keys derived from it.
sync_pb::NigoriSpecifics BuildCustomPassphraseNigoriSpecifics(
    const KeyParamsForTesting& passphrase_key_params,
    const std::optional<KeyParamsForTesting>& old_key_params = std::nullopt);

// Initializes KeyDerivationParams as described in a given |nigori|. This
// function will fail the test (using ADD_FAILURE/EXPECT) if the |nigori| is
// not a custom passphrase one.
KeyDerivationParams InitCustomPassphraseKeyDerivationParamsFromNigori(
    const sync_pb::NigoriSpecifics& nigori);

// Given a |nigori| with CUSTOM_PASSPHRASE passphrase type, initializes the
// Cryptographer with the key described in it. Since the key inside the Nigori
// is encrypted (by design), the provided |passphrase| will be used to
// decrypt it. This function will fail the test (using ADD_FAILURE/EXPECT) if
// the |nigori| is not a custom passphrase one, or if the key cannot be
// decrypted.
std::unique_ptr<Cryptographer> InitCustomPassphraseCryptographerFromNigori(
    const sync_pb::NigoriSpecifics& nigori,
    const std::string& passphrase);

// Returns an EntitySpecifics containing encrypted data corresponding to the
// provided BookmarkSpecifics and encrypted using the given |key_params|.
sync_pb::EntitySpecifics GetEncryptedBookmarkEntitySpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const KeyParamsForTesting& key_params);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_NIGORI_TEST_UTILS_H_
