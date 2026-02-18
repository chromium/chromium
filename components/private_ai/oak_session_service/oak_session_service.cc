// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/oak_session_service/oak_session_service.h"

#include "base/check_op.h"
#include "base/notimplemented.h"
#include "components/private_ai/crypto/secure_session_impl.h"
#include "components/private_ai/mojom/oak_session.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace private_ai {

OakSessionService::OakSessionService(
    mojo::PendingReceiver<mojom::OakSession> receiver)
    : receiver_(this, std::move(receiver)) {}

OakSessionService::~OakSessionService() = default;

void OakSessionService::InitiateHandshake(InitiateHandshakeCallback callback) {
  CHECK_EQ(handshake_order_, HandshakeStep::kStep0NotStarted);
  handshake_order_ = HandshakeStep::kStep1HandshakeInitiated;

  HandshakeMessage result = impl_.GetHandshakeMessage();
  std::move(callback).Run(std::move(result));
}

void OakSessionService::CompleteHandshake(HandshakeMessage response,
                                          CompleteHandshakeCallback callback) {
  CHECK_EQ(handshake_order_, HandshakeStep::kStep1HandshakeInitiated);
  handshake_order_ = HandshakeStep::kStep2HandshakeCompleted;

  bool result = impl_.ProcessHandshakeResponse(response);
  std::move(callback).Run(result);
}

void OakSessionService::Encrypt(const std::vector<uint8_t>& input,
                                EncryptCallback callback) {
  CHECK_EQ(handshake_order_, HandshakeStep::kStep2HandshakeCompleted);

  std::optional<std::vector<uint8_t>> result = impl_.Encrypt(input);
  std::move(callback).Run(result);
}

void OakSessionService::Decrypt(const std::vector<uint8_t>& input,
                                DecryptCallback callback) {
  CHECK_EQ(handshake_order_, HandshakeStep::kStep2HandshakeCompleted);

  std::optional<std::vector<uint8_t>> result = impl_.Decrypt(input);
  std::move(callback).Run(result);
}

}  // namespace private_ai
