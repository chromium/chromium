// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NIGORI_CRYPTOGRAPHER_H_
#define COMPONENTS_SYNC_ENGINE_NIGORI_CRYPTOGRAPHER_H_

#include <memory>
#include <string>

#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"
#include "components/sync/protocol/encryption.pb.h"

namespace syncer {

// Interface used to encrypt and decrypt sensitive sync data (eg. passwords).
class Cryptographer {
 public:
  Cryptographer();

  Cryptographer& operator=(const Cryptographer&) = delete;

  virtual ~Cryptographer();

  // Returns whether this cryptographer is ready to encrypt data, using
  // EncryptString(). This usually means that a default encryption key is
  // available and there are no pending keys.
  virtual bool CanEncrypt() const = 0;

  // Returns whether this cryptographer *should be* able to decrypt |encrypted|,
  // i.e. whether the key_name field in |encrypted| matches one of the known
  // keys. Calling DecryptToString() can still fail if the blob field is
  // corrupted.
  virtual bool CanDecrypt(const sync_pb::EncryptedData& encrypted) const = 0;

  // Returns a name that uniquely identifies the key used for encryption.
  virtual std::string GetDefaultEncryptionKeyName() const = 0;

  // Encrypted |decrypted| into |*encrypted|. |encrypted| must not be null.
  // Returns false in case of error, which most notably includes the case
  // where CanEncrypt() returns false.
  virtual bool EncryptString(const std::string& decrypted,
                             sync_pb::EncryptedData* encrypted) const = 0;

  // Decrypts |encrypted| as a plaintext decrypted data into |*decrypted|.
  // |decrypted| must not be null. Returns false in case of error, which most
  // notably includes the case where CanDecrypt() would have returned false.
  virtual bool DecryptToString(const sync_pb::EncryptedData& encrypted,
                               std::string* decrypted) const = 0;

  // For testing purposes only: returns the Public-private key-pair associated
  // with |version|.
  virtual const CrossUserSharingPublicPrivateKeyPair&
  GetCrossUserSharingKeyPairForTesting(uint32_t version) const = 0;

  // Convenience function to deal with protocol buffers. It uses EncryptString()
  // after serialization.
  bool Encrypt(const ::google::protobuf::MessageLite& message,
               sync_pb::EncryptedData* encrypted) const;

  // Convenience function to deal with protocol buffers. After decryption, it
  // parses the decrypted content into a protocol buffer.
  bool Decrypt(const sync_pb::EncryptedData& encrypted,
               ::google::protobuf::MessageLite* message) const;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_NIGORI_CRYPTOGRAPHER_H_
