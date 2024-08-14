// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/nigori/cryptographer.h"

#include "base/check.h"
#include "base/logging.h"
#include "components/sync/protocol/encryption.pb.h"

namespace syncer {

Cryptographer::Cryptographer() = default;

Cryptographer::~Cryptographer() = default;

bool Cryptographer::Encrypt(const ::google::protobuf::MessageLite& message,
                            sync_pb::EncryptedData* encrypted) const {
  DCHECK(encrypted);

  std::string serialized;
  if (!message.SerializeToString(&serialized)) {
    DLOG(ERROR) << "Message is invalid/missing a required field.";
    return false;
  }

  return EncryptString(serialized, encrypted);
}

bool Cryptographer::Decrypt(const sync_pb::EncryptedData& encrypted,
                            ::google::protobuf::MessageLite* message) const {
  DCHECK(message);

  std::string plaintext;
  if (!DecryptToString(encrypted, &plaintext)) {
    return false;
  }

  return message->ParseFromString(plaintext);
}

}  // namespace syncer
