// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_SECURE_SESSION_H_
#define COMPONENTS_LEGION_SECURE_SESSION_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "components/legion/legion_common.h"

namespace oak::session::v1 {
class EncryptedMessage;
class HandshakeRequest;
class HandshakeResponse;
}  // namespace oak::session::v1

namespace legion {

// Interface for secure session management.
// Handles cryptographic operations, including handshake, encryption, and
// decryption.
class SecureSession {
 public:
  using GetHandshakeMessageOnceCallback =
      base::OnceCallback<void(oak::session::v1::HandshakeRequest)>;

  using ProcessHandshakeResponseOnceCallback = base::OnceCallback<void(bool)>;

  using EncryptOnceCallback = base::OnceCallback<void(
      std::optional<oak::session::v1::EncryptedMessage>)>;

  using DecryptOnceCallback = base::OnceCallback<void(std::optional<Response>)>;

  virtual ~SecureSession() = default;

  // Generates the initial handshake message.
  //
  // Runs callback with `std::nullopt` on failure.
  virtual void GetHandshakeMessage(
      GetHandshakeMessageOnceCallback callback) = 0;

  // Processes the server's handshake response (e.g., keys).
  // This should be called after the initial handshake message has been sent
  // and a response has been received from the transport layer.
  //
  // Runs callback with `true` on success.
  virtual void ProcessHandshakeResponse(
      const oak::session::v1::HandshakeResponse& response,
      ProcessHandshakeResponseOnceCallback callback) = 0;

  // Encrypts the given data.
  // This should only be called after the handshake is complete.
  //
  // Runs callback with `std::nullopt` on failure.
  virtual void Encrypt(const Request& data, EncryptOnceCallback callback) = 0;

  // Decrypts the given data.
  // This should only be called after the handshake is complete.
  //
  // Runs callback with `std::nullopt` on failure.
  virtual void Decrypt(const oak::session::v1::EncryptedMessage& data,
                       DecryptOnceCallback callback) = 0;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_SECURE_SESSION_H_
