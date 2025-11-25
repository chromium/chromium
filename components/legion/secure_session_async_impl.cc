// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/secure_session_async_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "crypto/secure_session_impl.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace legion {

namespace {

oak::session::v1::HandshakeRequest ConvertToRequestProto(
    const HandshakeMessage& input) {
  oak::session::v1::HandshakeRequest output;
  output.mutable_noise_handshake_message()->set_ephemeral_public_key(
      input.ephemeral_public_key.data(), input.ephemeral_public_key.size());
  output.mutable_noise_handshake_message()->set_ciphertext(
      input.ciphertext.data(), input.ciphertext.size());
  return output;
}

std::optional<HandshakeMessage> ConvertToHandshakeMessage(
    const oak::session::v1::HandshakeResponse& response) {
  if (!response.has_noise_handshake_message()) {
    return std::nullopt;
  }

  const auto noise_msg = response.noise_handshake_message();

  HandshakeMessage output(
      std::vector<uint8_t>(noise_msg.ephemeral_public_key().begin(),
                           noise_msg.ephemeral_public_key().end()),
      std::vector<uint8_t>(noise_msg.ciphertext().begin(),
                           noise_msg.ciphertext().end()));
  return output;
}

std::optional<oak::session::v1::EncryptedMessage> ConvertToEncryptedMessage(
    const std::optional<std::vector<uint8_t>>& encrypted_data) {
  if (!encrypted_data.has_value()) {
    return std::nullopt;
  }

  oak::session::v1::EncryptedMessage encrypted_message;
  encrypted_message.set_ciphertext(encrypted_data.value().data(),
                                   encrypted_data.value().size());
  return encrypted_message;
}

std::vector<uint8_t> ConvertToBytes(
    const oak::session::v1::EncryptedMessage& encrypted_msg) {
  return std::vector<uint8_t>(encrypted_msg.ciphertext().begin(),
                              encrypted_msg.ciphertext().end());
}

}  // namespace

SecureSessionAsyncImpl::SecureSessionAsyncImpl() = default;

SecureSessionAsyncImpl::~SecureSessionAsyncImpl() = default;

void SecureSessionAsyncImpl::GetHandshakeMessage(
    SecureSession::GetHandshakeMessageOnceCallback callback) {
  auto result = ConvertToRequestProto(sync_impl_.GetHandshakeMessage());

  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), result));
}

void SecureSessionAsyncImpl::ProcessHandshakeResponse(
    const oak::session::v1::HandshakeResponse& response,
    SecureSession::ProcessHandshakeResponseOnceCallback callback) {
  auto handshake_msg = ConvertToHandshakeMessage(response);

  bool result = handshake_msg.has_value() &&
                sync_impl_.ProcessHandshakeResponse(handshake_msg.value());

  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  task_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), result));
}

void SecureSessionAsyncImpl::Encrypt(const Request& data,
                                     EncryptOnceCallback callback) {
  auto result = ConvertToEncryptedMessage(sync_impl_.Encrypt(data));

  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), std::move(result)));
}

void SecureSessionAsyncImpl::Decrypt(
    const oak::session::v1::EncryptedMessage& data,
    DecryptOnceCallback callback) {
  auto result = sync_impl_.Decrypt(ConvertToBytes(data));

  auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), std::move(result)));
}

void SecureSessionAsyncImpl::set_crypter_for_testing(
    std::unique_ptr<Crypter> crypter) {
  sync_impl_.set_crypter_for_testing(std::move(crypter));
}

}  // namespace legion
