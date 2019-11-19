// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_STATE_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_STATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/sync/nigori/nigori.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

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

  bool NeedsKeystoreKeyRotation() const;

  std::unique_ptr<CryptographerImpl> cryptographer;

  // Pending keys represent a remote update that contained a keybag that cannot
  // be decrypted (e.g. user needs to enter a custom passphrase). If pending
  // keys are present, |*cryptographer| does not have a default encryption key
  // set and instead the should-be default encryption key is determined by the
  // key in |pending_keys_|.
  base::Optional<sync_pb::EncryptedData> pending_keys;

  // TODO(mmoskvitin): Consider adopting the C++ enum PassphraseType here and
  // if so remove function ProtoPassphraseInt32ToProtoEnum() from
  // passphrase_enums.h.
  sync_pb::NigoriSpecifics::PassphraseType passphrase_type;
  base::Time keystore_migration_time;
  base::Time custom_passphrase_time;

  // The key derivation params we are using for the custom passphrase. Set iff
  // |passphrase_type| is CUSTOM_PASSPHRASE, otherwise key derivation method
  // is always PBKDF2.
  base::Optional<KeyDerivationParams> custom_passphrase_key_derivation_params;
  bool encrypt_everything;

  // Contains keystore keys. Uses last keystore key as encryption key. Must be
  // not null. Serialized as keystore keys, which must be encrypted with
  // OSCrypt before persisting.
  std::unique_ptr<KeystoreKeysCryptographer> keystore_keys_cryptographer;

  // Represents |keystore_decryptor_token| from NigoriSpecifics in case it
  // can't be decrypted right after remote update arrival due to lack of
  // keystore keys. May be set only for keystore Nigori.
  base::Optional<sync_pb::EncryptedData> pending_keystore_decryptor_token;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_STATE_H_
