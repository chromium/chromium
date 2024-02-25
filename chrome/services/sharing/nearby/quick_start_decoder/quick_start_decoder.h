// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_QUICK_START_DECODER_QUICK_START_DECODER_H_
#define CHROME_SERVICES_SHARING_NEARBY_QUICK_START_DECODER_QUICK_START_DECODER_H_

#include <optional>
#include <vector>

#include "base/types/expected.h"
#include "base/values.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-forward.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::quick_start {

// QuickStartDecoder is a class on the utility process that will
// accept incoming raw bytes from an Android device, decode the
// bytes and parse them into secure structs that can be consumed
// by the browser process.
class QuickStartDecoder : public mojom::QuickStartDecoder {
 public:
  QuickStartDecoder(mojo::PendingReceiver<mojom::QuickStartDecoder> receiver,
                    base::OnceClosure on_disconnect);
  QuickStartDecoder(const QuickStartDecoder&) = delete;
  QuickStartDecoder& operator=(const QuickStartDecoder&) = delete;
  ~QuickStartDecoder() override;

  // mojom::QuickStartDecoder;
  void DecodeQuickStartMessage(
      const std::optional<std::vector<uint8_t>>& data,
      DecodeQuickStartMessageCallback callback) override;

 private:
  friend class QuickStartDecoderTest;

  base::expected<mojom::QuickStartMessagePtr, mojom::QuickStartDecoderError>
  DoDecodeQuickStartMessage(const std::vector<uint8_t>& data);
  base::expected<mojom::QuickStartMessagePtr, mojom::QuickStartDecoderError>
  DecodeSecondDeviceAuthPayload(const base::Value::Dict& payload);
  base::expected<mojom::QuickStartMessagePtr, mojom::QuickStartDecoderError>
  DecodeBootstrapConfigurations(const base::Value::Dict& payload);
  base::expected<mojom::QuickStartMessagePtr, mojom::QuickStartDecoderError>
  DecodeQuickStartPayload(const base::Value::Dict& payload);
  base::expected<mojom::QuickStartMessagePtr, mojom::QuickStartDecoderError>
  DecodeWifiCredentials(const base::Value::Dict& wifi_network_information);

  mojo::Receiver<mojom::QuickStartDecoder> receiver_;
};

}  // namespace ash::quick_start

#endif  // CHROME_SERVICES_SHARING_NEARBY_QUICK_START_DECODER_QUICK_START_DECODER_H_
