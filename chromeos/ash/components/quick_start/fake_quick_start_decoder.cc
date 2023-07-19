// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_quick_start_decoder.h"

#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-forward.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  EXPECT_EQ(expected_data_, data);
  if (error_.has_value()) {
    std::move(callback).Run(nullptr, error_);
  }

  std::move(callback).Run(std::move(fido_assertion_), absl::nullopt);
}

void FakeQuickStartDecoder::DecodeNotifySourceOfUpdateResponse(
    const absl::optional<std::vector<uint8_t>>& data,
    DecodeNotifySourceOfUpdateResponseCallback callback) {
  std::move(callback).Run(/*ack_received=*/notify_source_of_update_response_);
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
    absl::optional<bool> ack_received) {
  notify_source_of_update_response_ = ack_received;
}

void FakeQuickStartDecoder::SetBootstrapConfigurationsResponse(
    const std::string& cryptauth_device_id,
    absl::optional<mojom::QuickStartDecoderError> error) {
  response_cryptauth_device_id_ = cryptauth_device_id;
  error_ = error;
}

}  // namespace ash::quick_start
