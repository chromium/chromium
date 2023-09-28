// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_QUICK_START_FAKE_QUICK_START_DECODER_H_
#define CHROMEOS_ASH_COMPONENTS_QUICK_START_FAKE_QUICK_START_DECODER_H_

#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-forward.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::quick_start {

class FakeQuickStartDecoder : public mojom::QuickStartDecoder {
 public:
  FakeQuickStartDecoder();
  FakeQuickStartDecoder(const FakeQuickStartDecoder&) = delete;
  FakeQuickStartDecoder(FakeQuickStartDecoder&&) = delete;
  ~FakeQuickStartDecoder() override;

  mojo::PendingRemote<mojom::QuickStartDecoder> GetRemote();

  // mojom::QuickStartDecoder:
  void DecodeBootstrapConfigurations(
      const absl::optional<std::vector<uint8_t>>& data,
      DecodeBootstrapConfigurationsCallback callback) override;
  void DecodeGetAssertionResponse(
      const absl::optional<std::vector<uint8_t>>& data,
      DecodeGetAssertionResponseCallback callback) override;
  void DecodeWifiCredentialsResponse(
      const absl::optional<std::vector<uint8_t>>& data,
      DecodeWifiCredentialsResponseCallback callback) override;
  void DecodeNotifySourceOfUpdateResponse(
      const absl::optional<std::vector<uint8_t>>& data,
      DecodeNotifySourceOfUpdateResponseCallback callback) override;
  void DecodeUserVerificationMethod(
      const absl::optional<std::vector<uint8_t>>& data,
      DecodeUserVerificationMethodCallback callback) override;
  void DecodeUserVerificationRequested(
      const absl::optional<std::vector<uint8_t>>& data,
      DecodeUserVerificationRequestedCallback callback) override;
  void DecodeUserVerificationResult(
      const absl::optional<std::vector<uint8_t>>& data,
      DecodeUserVerificationResultCallback callback) override;
  void DecodeQuickStartMessage(
      const absl::optional<std::vector<uint8_t>>& data,
      DecodeQuickStartMessageCallback callback) override;

  void SetExpectedData(std::vector<uint8_t> expected_data);
  void SetAssertionResponse(mojom::FidoAssertionResponsePtr fido_assertion);

  void SetUserVerificationResponse(mojom::UserVerificationResult result,
                                   bool is_first_user_verification);

  void SetUserVerificationRequested(bool is_awaiting_user_verification);

  void SetDecoderError(mojom::QuickStartDecoderError error);

  void SetWifiCredentialsResponse(
      mojom::WifiCredentialsPtr credentials,
      absl::optional<mojom::QuickStartDecoderError> error);

  void SetNotifySourceOfUpdateResponse(
      mojom::NotifySourceOfUpdateResponsePtr notify_source_of_update_response);

  void SetBootstrapConfigurationsResponse(
      const std::string& cryptauth_device_id,
      absl::optional<mojom::QuickStartDecoderError> error);

  void SetQuickStartMessage(mojom::QuickStartMessagePtr quick_start_message);

 private:
  std::vector<uint8_t> expected_data_;
  mojo::ReceiverSet<ash::quick_start::mojom::QuickStartDecoder> receiver_set_;
  mojom::NotifySourceOfUpdateResponsePtr notify_source_of_update_response_;
  mojom::WifiCredentialsPtr credentials_;
  mojom::FidoAssertionResponsePtr fido_assertion_;
  mojom::UserVerificationMethodPtr user_verification_method_;
  mojom::UserVerificationRequestedPtr user_verification_request_;
  mojom::UserVerificationResponsePtr user_verification_response_;
  mojom::QuickStartMessagePtr quick_start_message_;
  absl::optional<mojom::QuickStartDecoderError> error_;
  std::string response_cryptauth_device_id_;
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_QUICK_START_FAKE_QUICK_START_DECODER_H_
