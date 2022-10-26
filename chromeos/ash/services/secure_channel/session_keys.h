// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SESSION_KEYS_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SESSION_KEYS_H_

#include <string>

namespace ash::secure_channel {

// This class contains the secure channel (secure context) session keys. This
// class derives (from the session symmetric key) different keys for encryption
// and decryption. The protocol initiator (i.e. the Chromebook) should use
// |initiator_encode_key()| to encrypt the messages, and the responder should
// use |responder_encode_key()|. This is reversed for decryption.
class SessionKeys {
 public:
  // Create session keys derived from the |session_symmetric_key|.
  explicit SessionKeys(const std::string& session_symmetric_key);

  SessionKeys();

  virtual ~SessionKeys();

  virtual std::string initiator_encode_key() const;
  virtual std::string responder_encode_key() const;

 private:
  // The initiator encoding key.
  std::string initiator_encode_key_;

  // The responder encoding key.
  std::string responder_encode_key_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SESSION_KEYS_H_
