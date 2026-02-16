// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_OAK_SESSION_SERVICE_OAK_SESSION_SERVICE_H_
#define COMPONENTS_LEGION_OAK_SESSION_SERVICE_OAK_SESSION_SERVICE_H_

#include <cstdint>
#include <vector>

#include "components/legion/crypto/handshake_message.h"
#include "components/legion/crypto/secure_session_impl.h"
#include "components/legion/mojom/oak_session.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace private_ai {

// This class should be run in a sandboxed process.
//
// It implements the mojom::OakSession interface to that effect.
class OakSessionService : public mojom::OakSession {
 public:
  explicit OakSessionService(mojo::PendingReceiver<mojom::OakSession> receiver);
  ~OakSessionService() override;

  OakSessionService(const OakSessionService&) = delete;
  OakSessionService& operator=(const OakSessionService&) = delete;

  // mojom::OakSession:
  void InitiateHandshake(InitiateHandshakeCallback callback) override;
  void CompleteHandshake(HandshakeMessage response,
                         CompleteHandshakeCallback callback) override;
  void Encrypt(const std::vector<uint8_t>& input,
               EncryptCallback callback) override;
  void Decrypt(const std::vector<uint8_t>& input,
               DecryptCallback callback) override;

 private:
  enum class HandshakeStep {
    kStep0NotStarted,
    kStep1HandshakeInitiated,
    kStep2HandshakeCompleted
  };
  HandshakeStep handshake_order_ = HandshakeStep::kStep0NotStarted;

  SecureSessionImpl impl_;
  mojo::Receiver<mojom::OakSession> receiver_;
};

}  // namespace private_ai

#endif  // COMPONENTS_LEGION_OAK_SESSION_SERVICE_OAK_SESSION_SERVICE_H_
