// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_SECURE_SESSION_ASYNC_IMPL_H_
#define COMPONENTS_LEGION_SECURE_SESSION_ASYNC_IMPL_H_

#include <memory>
#include <optional>

#include "components/legion/crypto/secure_session_impl.h"
#include "components/legion/secure_session.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace legion {

class SecureSessionAsyncImpl : public SecureSession {
 public:
  SecureSessionAsyncImpl();
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

  void set_crypter_for_testing(std::unique_ptr<Crypter> crypter);

 private:
  SecureSessionImpl sync_impl_;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_SECURE_SESSION_ASYNC_IMPL_H_
