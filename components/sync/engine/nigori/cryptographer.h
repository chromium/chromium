// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_NIGORI_CRYPTOGRAPHER_H_
#define COMPONENTS_SYNC_ENGINE_NIGORI_CRYPTOGRAPHER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace sync_pb {
class EncryptedData;
}  // namespace sync_pb

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

  // Encrypts |plaintext| using Auth HPKE using |recipient_public_key|.
  // Authentication is added with the current sender's authentication key.
  // Empty optional is returned upon failure.
  virtual std::optional<std::vector<uint8_t>> AuthEncryptForCrossUserSharing(
      base::span<const uint8_t> plaintext,
      base::span<const uint8_t> recipient_public_key) const = 0;

  // Decrypts |encrypted_data| using Auth HPKE using the keys corresponding
  // to |recipient_key_version| and authenticates that the sender actually used
  // |sender_public_key| upon auth encryption.
  // Empty optional is returned upon failure.
  virtual std::optional<std::vector<uint8_t>> AuthDecryptForCrossUserSharing(
      base::span<const uint8_t> encrypted_data,
      base::span<const uint8_t> sender_public_key,
      const uint32_t recipient_key_version) const = 0;

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
