// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_SECURE_SESSION_ASYNC_IMPL_H_
#define COMPONENTS_PRIVATE_AI_SECURE_SESSION_ASYNC_IMPL_H_

#include "components/private_ai/mojom/oak_session.mojom.h"
#include "components/private_ai/private_ai_oak_session_driver.h"
#include "components/private_ai/secure_session.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace private_ai {

class SecureSessionAsyncImpl : public SecureSession {
 public:
  explicit SecureSessionAsyncImpl(
      PrivateAiOakSessionDriver* oak_session_driver);
  ~SecureSessionAsyncImpl() override;

  // SecureSession:
  void GetHandshakeMessage(
      SecureSession::GetHandshakeMessageOnceCallback callback) override;
  void ProcessHandshakeResponse(
      const oak::session::v1::HandshakeResponse& response,
      ProcessHandshakeResponseOnceCallback callback) override;
  void Encrypt(const Request& data, EncryptOnceCallback callback) override;
  void Decrypt(const oak::session::v1::EncryptedMessage& data,
               DecryptOnceCallback callback) override;

 private:
  mojo::Remote<mojom::OakSession> service_;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_SECURE_SESSION_ASYNC_IMPL_H_
