// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_quick_start_decoder.h"

#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-forward.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"

namespace ash::quick_start {

FakeQuickStartDecoder::~FakeQuickStartDecoder() = default;

FakeQuickStartDecoder::FakeQuickStartDecoder() = default;

mojo::PendingRemote<mojom::QuickStartDecoder>
FakeQuickStartDecoder::GetRemote() {
  mojo::PendingRemote<mojom::QuickStartDecoder> pending_remote;
  receiver_set_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}
void FakeQuickStartDecoder::DecodeBootstrapConfigurations(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeBootstrapConfigurationsCallback callback) {
  std::move(callback).Run(
      mojom::BootstrapConfigurations::New(response_cryptauth_device_id_),
      error_);
}

void FakeQuickStartDecoder::DecodeWifiCredentialsResponse(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeWifiCredentialsResponseCallback callback) {
  std::move(callback).Run(std::move(credentials_), error_);
}

void FakeQuickStartDecoder::DecodeUserVerificationMethod(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeUserVerificationMethodCallback callback) {
  if (error_ != absl::nullopt) {
    std::move(callback).Run(nullptr, error_);
  } else {
    std::move(callback).Run(std::move(user_verification_method_),
                            absl::nullopt);
  }
}

void FakeQuickStartDecoder::DecodeUserVerificationRequested(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeUserVerificationRequestedCallback callback) {
  if (error_ != absl::nullopt) {
    std::move(callback).Run(nullptr, error_);
  } else {
    std::move(callback).Run(std::move(user_verification_request_),
                            absl::nullopt);
  }
}

void FakeQuickStartDecoder::DecodeUserVerificationResult(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeUserVerificationResultCallback callback) {
  if (error_ != absl::nullopt) {
    std::move(callback).Run(nullptr, error_);
  } else {
    std::move(callback).Run(std::move(user_verification_response_),
                            absl::nullopt);
  }
}

void FakeQuickStartDecoder::DecodeGetAssertionResponse(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeGetAssertionResponseCallback callback) {
  if (error_.has_value()) {
    std::move(callback).Run(nullptr, error_);
    return;
  }

  std::move(callback).Run(std::move(fido_assertion_), absl::nullopt);
}

void FakeQuickStartDecoder::DecodeNotifySourceOfUpdateResponse(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeNotifySourceOfUpdateResponseCallback callback) {
  if (error_.has_value()) {
    std::move(callback).Run(nullptr, error_);
    return;
  }

  std::move(callback).Run(std::move(notify_source_of_update_response_),
                          absl::nullopt);
}

void FakeQuickStartDecoder::DecodeQuickStartMessage(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeQuickStartMessageCallback callback) {
  if (error_ != absl::nullopt) {
    std::move(callback).Run(nullptr, error_);
  } else {
    std::move(callback).Run(std::move(quick_start_message_), absl::nullopt);
  }
}

void FakeQuickStartDecoder::SetUserVerificationRequested(
    bool is_awaiting_user_verification) {
  user_verification_request_ =
      mojom::UserVerificationRequested::New(is_awaiting_user_verification);
}

void FakeQuickStartDecoder::SetExpectedData(
    std::vector<uint8_t> expected_data) {
  expected_data_ = expected_data;
}

void FakeQuickStartDecoder::SetDecoderError(
    mojom::QuickStartDecoderError error) {
  error_ = error;
}

void FakeQuickStartDecoder::SetUserVerificationResponse(
    mojom::UserVerificationResult result,
    bool is_first_user_verification) {
  user_verification_response_ =
      mojom::UserVerificationResponse::New(result, is_first_user_verification);
}

void FakeQuickStartDecoder::SetAssertionResponse(
    mojom::FidoAssertionResponsePtr fido_assertion) {
  fido_assertion_ = std::move(fido_assertion);
}

void FakeQuickStartDecoder::SetWifiCredentialsResponse(
    mojom::WifiCredentialsPtr credentials,
    absl::optional<mojom::QuickStartDecoderError> error) {
  credentials_ = std::move(credentials);
  error_ = error;
}

void FakeQuickStartDecoder::SetNotifySourceOfUpdateResponse(
    mojom::NotifySourceOfUpdateResponsePtr notify_source_of_update_response) {
  notify_source_of_update_response_ =
      std::move(notify_source_of_update_response);
}

void FakeQuickStartDecoder::SetBootstrapConfigurationsResponse(
    const std::string& cryptauth_device_id,
    absl::optional<mojom::QuickStartDecoderError> error) {
  response_cryptauth_device_id_ = cryptauth_device_id;
  error_ = error;
}

void FakeQuickStartDecoder::SetQuickStartMessage(
    mojom::QuickStartMessagePtr quick_start_message) {
  quick_start_message_ = std::move(quick_start_message);
}

}  // namespace ash::quick_start
