// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/secure_session_async_impl.h"

#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "components/private_ai/crypto/constants.h"
#include "components/private_ai/mojom/oak_session.mojom.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/oak/chromium/proto/session/session.pb.h"

namespace private_ai {

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

  if (noise_msg.ephemeral_public_key().size() != kP256X962Length) {
    return std::nullopt;
  }

  std::array<uint8_t, kP256X962Length> ephemeral_public_key;
  base::span(ephemeral_public_key)
      .copy_from(base::as_byte_span(noise_msg.ephemeral_public_key()));

  HandshakeMessage output(
      ephemeral_public_key,
      base::ToVector(base::as_byte_span(noise_msg.ciphertext())));
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

// static
std::unique_ptr<SecureSessionAsyncImpl>
SecureSessionAsyncImpl::CreateForTesting(  // IN-TEST
    mojo::Remote<mojom::OakSession> service) {
  return base::WrapUnique(new SecureSessionAsyncImpl(std::move(service)));
}

SecureSessionAsyncImpl::SecureSessionAsyncImpl(
    mojo::Remote<mojom::OakSession> service)
    : service_(std::move(service)) {}

SecureSessionAsyncImpl::SecureSessionAsyncImpl()
    : service_(content::ServiceProcessHost::Launch<mojom::OakSession>(
          content::ServiceProcessHost::Options()
              .WithDisplayName("Oak Session Service")
              .Pass())) {}

SecureSessionAsyncImpl::~SecureSessionAsyncImpl() = default;

void SecureSessionAsyncImpl::GetHandshakeMessage(
    SecureSession::GetHandshakeMessageOnceCallback original_callback) {
  auto split_callback = base::SplitOnceCallback(std::move(original_callback));

  auto callback = mojo::WrapCallbackWithDropHandler(
      base::BindOnce(
          [](GetHandshakeMessageOnceCallback callback,
             HandshakeMessage message) {
            base::UmaHistogramBoolean(
                "PrivateAi.OakSessionSandboxStability.InitiateHandshake", true);
            std::move(callback).Run(ConvertToRequestProto(message));
          },
          std::move(split_callback.first)),
      base::BindOnce(
          [](GetHandshakeMessageOnceCallback callback) {
            base::UmaHistogramBoolean(
                "PrivateAi.OakSessionSandboxStability.InitiateHandshake",
                false);
            std::move(callback).Run(std::nullopt);
          },
          std::move(split_callback.second)));

  service_->InitiateHandshake(std::move(callback));
}

void SecureSessionAsyncImpl::ProcessHandshakeResponse(
    const oak::session::v1::HandshakeResponse& response,
    ProcessHandshakeResponseOnceCallback original_callback) {
  auto handshake_msg = ConvertToHandshakeMessage(response);

  if (!handshake_msg.has_value()) {
    std::move(original_callback).Run(false);
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(original_callback));

  auto callback = mojo::WrapCallbackWithDropHandler(
      base::BindOnce(
          [](ProcessHandshakeResponseOnceCallback callback, bool result) {
            base::UmaHistogramBoolean(
                "PrivateAi.OakSessionSandboxStability.CompleteHandshake", true);
            std::move(callback).Run(result);
          },
          std::move(split_callback.first)),
      base::BindOnce(
          [](ProcessHandshakeResponseOnceCallback callback) {
            base::UmaHistogramBoolean(
                "PrivateAi.OakSessionSandboxStability.CompleteHandshake",
                false);
            std::move(callback).Run(false);
          },
          std::move(split_callback.second)));

  service_->CompleteHandshake(std::move(handshake_msg.value()),
                              std::move(callback));
}

void SecureSessionAsyncImpl::Encrypt(const Request& data,
                                     EncryptOnceCallback original_callback) {
  auto split_callback = base::SplitOnceCallback(std::move(original_callback));

  auto callback = mojo::WrapCallbackWithDropHandler(
      base::BindOnce(
          [](EncryptOnceCallback callback,
             const std::optional<std::vector<uint8_t>>& encrypted_data) {
            base::UmaHistogramBoolean(
                "PrivateAi.OakSessionSandboxStability.Encrypt", true);
            std::move(callback).Run(ConvertToEncryptedMessage(encrypted_data));
          },
          std::move(split_callback.first)),
      base::BindOnce(
          [](EncryptOnceCallback callback) {
            base::UmaHistogramBoolean(
                "PrivateAi.OakSessionSandboxStability.Encrypt", false);
            std::move(callback).Run(std::nullopt);
          },
          std::move(split_callback.second)));

  service_->Encrypt(data, std::move(callback));
}

void SecureSessionAsyncImpl::Decrypt(
    const oak::session::v1::EncryptedMessage& data,
    DecryptOnceCallback original_callback) {
  auto split_callback = base::SplitOnceCallback(std::move(original_callback));

  auto callback = mojo::WrapCallbackWithDropHandler(
      base::BindOnce(
          [](DecryptOnceCallback callback,
             const std::optional<std::vector<uint8_t>>& decrypted_data) {
            base::UmaHistogramBoolean(
                "PrivateAi.OakSessionSandboxStability.Decrypt", true);
            std::move(callback).Run(decrypted_data);
          },
          std::move(split_callback.first)),
      base::BindOnce(
          [](DecryptOnceCallback callback) {
            base::UmaHistogramBoolean(
                "PrivateAi.OakSessionSandboxStability.Decrypt", false);
            std::move(callback).Run(std::nullopt);
          },
          std::move(split_callback.second)));

  service_->Decrypt(ConvertToBytes(data), std::move(callback));
}

}  // namespace private_ai
