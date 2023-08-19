// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_STATE_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_STATE_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_key.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sync_pb {
class NigoriModel;
}  // namespace sync_pb

namespace syncer {

class CryptographerImpl;
class KeystoreKeysCryptographer;

struct NigoriState {
  static constexpr sync_pb::NigoriSpecifics::PassphraseType
      kInitialPassphraseType = sync_pb::NigoriSpecifics::UNKNOWN;

  static constexpr bool kInitialEncryptEverything = false;

  // Deserialization from proto.
  static NigoriState CreateFromLocalProto(const sync_pb::NigoriModel& proto);

  NigoriState();
  NigoriState(NigoriState&& other);
  ~NigoriState();

  NigoriState& operator=(NigoriState&& other);

  // Serialization to proto as persisted on local disk.
  sync_pb::NigoriModel ToLocalProto() const;

  // Serialization to proto as sent to the sync server.
  sync_pb::NigoriSpecifics ToSpecificsProto() const;

  // Makes a deep copy of |this|.
  NigoriState Clone() const;

  bool NeedsKeystoreReencryption() const;

  ModelTypeSet GetEncryptedTypes() const;

  // TODO(crbug.com/1109221): Make this const unique_ptr to avoid the object
  // being destroyed after it's been injected to the ModelTypeWorker-s.
  std::unique_ptr<CryptographerImpl> cryptographer;

  // Pending keys represent a remote update that contained a keybag that cannot
  // be decrypted (e.g. user needs to enter a custom passphrase). If pending
  // keys are present, |*cryptographer| does not have a default encryption key
  // set and instead the should-be default encryption key is determined by the
  // key in |pending_keys_|.
  absl::optional<sync_pb::EncryptedData> pending_keys;

  // TODO(mmoskvitin): Consider adopting the C++ enum PassphraseType here and
  // if so remove function ProtoPassphraseInt32ToProtoEnum() from
  // passphrase_enums.h.
  sync_pb::NigoriSpecifics::PassphraseType passphrase_type;
  base::Time keystore_migration_time;
  base::Time custom_passphrase_time;

  // The key derivation params we are using for the custom passphrase. Set iff
  // |passphrase_type| is CUSTOM_PASSPHRASE, otherwise key derivation method
  // is always PBKDF2.
  absl::optional<KeyDerivationParams> custom_passphrase_key_derivation_params;
  bool encrypt_everything;

  // Contains keystore keys. Uses last keystore key as encryption key. Must be
  // not null. Serialized as keystore keys, which must be encrypted with
  // OSCrypt before persisting.
  std::unique_ptr<KeystoreKeysCryptographer> keystore_keys_cryptographer;

  // Represents |keystore_decryptor_token| from NigoriSpecifics in case it
  // can't be decrypted right after remote update arrival due to lack of
  // keystore keys. May be set only for keystore Nigori.
  absl::optional<sync_pb::EncryptedData> pending_keystore_decryptor_token;

  // The name of the latest available trusted vault key that was used as the
  // default encryption key.
  absl::optional<std::string> last_default_trusted_vault_key_name;

  // Some debug-only fields for passphrase type TRUSTED_VAULT_PASSPHRASE.
  sync_pb::NigoriSpecifics::TrustedVaultDebugInfo trusted_vault_debug_info;

  // Current Public-key.
  absl::optional<CrossUserSharingPublicKey> cross_user_sharing_public_key;
  // Current Public-key version.
  absl::optional<uint32_t> cross_user_sharing_key_pair_version;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_STATE_H_
