// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_CRYPTOGRAPHER_H_
#define COMPONENTS_SYNC_NIGORI_CRYPTOGRAPHER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/sync/protocol/encryption.pb.h"

namespace syncer {

// Interface used to encrypt and decrypt sensitive sync data (eg. passwords).
class Cryptographer {
 public:
  Cryptographer();
  virtual ~Cryptographer();

  virtual std::unique_ptr<Cryptographer> Clone() const = 0;

  // Returns whether this cryptographer is ready to encrypt data, using
  // EncryptString(). This usually means that a default encryption key is
  // available and there are no pending keys.
  virtual bool CanEncrypt() const = 0;

  // Returns whether this cryptographer can decrypt |encrypted| using any of
  // the known keys.
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

  // Convenience function to deal with protocol buffers. It uses EncryptString()
  // after serialization.
  bool Encrypt(const ::google::protobuf::MessageLite& message,
               sync_pb::EncryptedData* encrypted) const;

  // Convenience function to deal with protocol buffers. After decryption, it
  // parses the decrypted content into a protocol buffer.
  bool Decrypt(const sync_pb::EncryptedData& encrypted,
               ::google::protobuf::MessageLite* message) const;

 private:
  DISALLOW_ASSIGN(Cryptographer);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_CRYPTOGRAPHER_H_
