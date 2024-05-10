// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENCRYPTED_MESSAGES_MESSAGE_ENCRYPTER_H_
#define COMPONENTS_ENCRYPTED_MESSAGES_MESSAGE_ENCRYPTER_H_

#include <stdint.h>

#include <string>
#include <string_view>

namespace encrypted_messages {

class EncryptedMessage;

// Messages are encrypted with a secret derived from a client key pair (which
// should be generated fresh randomly for each message) and a server public
// key. The remote message recipient can decrypt the message by performing the
// same key exchange using the client public key (included in EncryptedMessage)
// to recover the shared secret.
bool EncryptSerializedMessage(const uint8_t* server_public_key,
                              uint32_t server_public_key_version,
                              std::string_view hkdf_label,
                              const std::string& message,
                              EncryptedMessage* encrypted_message);

// Decrypts a message that was encrypted using the above function.
// Used only by tests.
bool DecryptMessageForTesting(const uint8_t server_private_key[32],
                              std::string_view hkdf_label,
                              const EncryptedMessage& encrypted_message,
                              std::string* decrypted_serialized_message);

}  // namespace encrypted_messages

#endif  // COMPONENTS_ENCRYPTED_MESSAGES_MESSAGE_ENCRYPTER_H_
