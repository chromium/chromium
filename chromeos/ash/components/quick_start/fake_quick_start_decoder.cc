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

void FakeQuickStartDecoder::DecodeQuickStartMessage(
    const std::optional<std::vector<uint8_t>>& data,
    DecodeQuickStartMessageCallback callback) {
  has_decode_been_called_ = true;
  CHECK(!results_.empty());
  auto [quick_start_message, error] = std::move(results_.front());
  results_.pop();
  if (error != std::nullopt) {
    std::move(callback).Run(nullptr, error);
  } else {
    std::move(callback).Run(std::move(quick_start_message), std::nullopt);
  }
}

void FakeQuickStartDecoder::SetUserVerificationRequested(
    bool is_awaiting_user_verification) {
  SetQuickStartMessage(mojom::QuickStartMessage::NewUserVerificationRequested(
      mojom::UserVerificationRequested::New(is_awaiting_user_verification)));
}

void FakeQuickStartDecoder::SetUserVerificationMethod(
    bool use_source_lock_screen_prompt) {
  SetQuickStartMessage(mojom::QuickStartMessage::NewUserVerificationMethod(
      mojom::UserVerificationMethod::New(use_source_lock_screen_prompt)));
}

void FakeQuickStartDecoder::SetUserVerificationResponse(
    mojom::UserVerificationResult result,
    bool is_first_user_verification) {
  SetQuickStartMessage(mojom::QuickStartMessage::NewUserVerificationResponse(
      mojom::UserVerificationResponse::New(result,
                                           is_first_user_verification)));
}

void FakeQuickStartDecoder::SetAssertionResponse(
    mojom::FidoAssertionResponsePtr fido_assertion) {
  SetQuickStartMessage(mojom::QuickStartMessage::NewFidoAssertionResponse(
      std::move(fido_assertion)));
}

void FakeQuickStartDecoder::SetWifiCredentialsResponse(
    mojom::WifiCredentialsPtr credentials) {
  SetQuickStartMessage(
      mojom::QuickStartMessage::NewWifiCredentials(std::move(credentials)));
}

void FakeQuickStartDecoder::SetNotifySourceOfUpdateResponse(
    mojom::NotifySourceOfUpdateResponsePtr notify_source_of_update_response) {
  SetQuickStartMessage(
      mojom::QuickStartMessage::NewNotifySourceOfUpdateResponse(
          std::move(notify_source_of_update_response)));
}

void FakeQuickStartDecoder::SetBootstrapConfigurationsResponse(
    const std::string& instance_id,
    const bool is_supervised_account,
    const std::string& email) {
  SetQuickStartMessage(mojom::QuickStartMessage::NewBootstrapConfigurations(
      mojom::BootstrapConfigurations::New(instance_id, is_supervised_account,
                                          email)));
}

void FakeQuickStartDecoder::SetQuickStartMessage(
    mojom::QuickStartMessagePtr quick_start_message) {
  results_.emplace(std::move(quick_start_message), std::nullopt);
}

void FakeQuickStartDecoder::SetDecoderError(
    mojom::QuickStartDecoderError error) {
  results_.emplace(nullptr, error);
}

}  // namespace ash::quick_start
