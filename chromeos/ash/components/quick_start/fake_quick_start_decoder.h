// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_QUICK_START_FAKE_QUICK_START_DECODER_H_
#define CHROMEOS_ASH_COMPONENTS_QUICK_START_FAKE_QUICK_START_DECODER_H_

#include <optional>

#include "base/containers/queue.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-forward.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::quick_start {

class FakeQuickStartDecoder : public mojom::QuickStartDecoder {
 public:
  FakeQuickStartDecoder();
  FakeQuickStartDecoder(const FakeQuickStartDecoder&) = delete;
  FakeQuickStartDecoder(FakeQuickStartDecoder&&) = delete;
  ~FakeQuickStartDecoder() override;

  mojo::PendingRemote<mojom::QuickStartDecoder> GetRemote();

  // mojom::QuickStartDecoder:
  void DecodeQuickStartMessage(
      const std::optional<std::vector<uint8_t>>& data,
      DecodeQuickStartMessageCallback callback) override;

  void SetAssertionResponse(mojom::FidoAssertionResponsePtr fido_assertion);

  void SetUserVerificationResponse(mojom::UserVerificationResult result,
                                   bool is_first_user_verification);

  void SetUserVerificationMethod(bool use_source_lock_screen_prompt);

  void SetUserVerificationRequested(bool is_awaiting_user_verification);

  void SetDecoderError(mojom::QuickStartDecoderError error);

  void SetWifiCredentialsResponse(mojom::WifiCredentialsPtr credentials);

  void SetNotifySourceOfUpdateResponse(
      mojom::NotifySourceOfUpdateResponsePtr notify_source_of_update_response);

  void SetBootstrapConfigurationsResponse(const std::string& instance_id,
                                          const bool is_supervised_account,
                                          const std::string& email);

  void SetQuickStartMessage(mojom::QuickStartMessagePtr quick_start_message);

  bool has_decode_been_called() { return has_decode_been_called_; }

 private:
  mojo::ReceiverSet<ash::quick_start::mojom::QuickStartDecoder> receiver_set_;
  base::queue<std::pair<mojom::QuickStartMessagePtr,
                        std::optional<mojom::QuickStartDecoderError>>>
      results_;
  bool has_decode_been_called_ = false;
};

}  // namespace ash::quick_start

#endif  // CHROMEOS_ASH_COMPONENTS_QUICK_START_FAKE_QUICK_START_DECODER_H_
