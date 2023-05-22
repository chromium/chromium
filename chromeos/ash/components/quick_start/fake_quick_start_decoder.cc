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
    const std::vector<uint8_t>& data,
    DecodeBootstrapConfigurationsCallback callback) {
  std::move(callback).Run(
      mojom::BootstrapConfigurations::New(response_cryptauth_device_id_),
      error_);
}

void FakeQuickStartDecoder::DecodeWifiCredentialsResponse(
    const std::vector<uint8_t>& data,
    DecodeWifiCredentialsResponseCallback callback) {
  std::move(callback).Run(std::move(credentials_), error_);
}

void FakeQuickStartDecoder::DecodeUserVerificationRequested(
    const std::vector<uint8_t>& data,
    DecodeUserVerificationRequestedCallback callback) {
  if (error_ != absl::nullopt) {
    std::move(callback).Run(nullptr, error_);
  } else {
    std::move(callback).Run(std::move(user_verification_request_),
                            absl::nullopt);
  }
}

void FakeQuickStartDecoder::DecodeUserVerificationResult(
    const std::vector<uint8_t>& data,
    DecodeUserVerificationResultCallback callback) {
  if (error_ != absl::nullopt) {
    std::move(callback).Run(nullptr, error_);
  } else {
    std::move(callback).Run(std::move(user_verification_response_),
                            absl::nullopt);
  }
}

void FakeQuickStartDecoder::DecodeGetAssertionResponse(
    const std::vector<uint8_t>& data,
    DecodeGetAssertionResponseCallback callback) {
  EXPECT_EQ(expected_data_, data);
  std::move(callback).Run(mojom::GetAssertionResponse::New(
      response_status_, response_decoder_status_, response_decoder_error_,
      response_email_, response_credential_id_, response_data_,
      response_signature_));
}

void FakeQuickStartDecoder::DecodeNotifySourceOfUpdateResponse(
    const std::vector<uint8_t>& data,
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
    mojom::GetAssertionResponse::GetAssertionStatus status,
    uint8_t decoder_status,
    uint8_t decoder_error,
    const std::string& email,
    const std::string& credential_id,
    const std::vector<uint8_t>& signature,
    const std::vector<uint8_t>& data) {
  response_status_ = status;
  response_decoder_status_ = decoder_status;
  response_decoder_error_ = decoder_error;
  response_email_ = email;
  response_credential_id_ = credential_id;
  response_signature_ = signature;
  response_data_ = data;
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
