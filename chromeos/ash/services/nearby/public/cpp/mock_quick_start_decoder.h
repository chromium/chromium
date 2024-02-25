// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_QUICK_START_DECODER_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_QUICK_START_DECODER_H_

#include <optional>

#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace nearby {

class MockQuickStartDecoder
    : public ash::quick_start::mojom::QuickStartDecoder {
 public:
  MockQuickStartDecoder();
  MockQuickStartDecoder(const MockQuickStartDecoder&) = delete;
  MockQuickStartDecoder(MockQuickStartDecoder&&) = delete;
  ~MockQuickStartDecoder() override;

  const mojo::SharedRemote<quick_start::mojom::QuickStartDecoder>&
  shared_remote() const {
    return shared_remote_;
  }

  void BindInterface(
      mojo::PendingReceiver<quick_start::mojom::QuickStartDecoder>
          pending_receiver);

  MOCK_METHOD(void,
              DecodeQuickStartMessage,
              (const std::optional<std::vector<uint8_t>>& data,
               DecodeQuickStartMessageCallback callback),
              (override));

 private:
  mojo::ReceiverSet<ash::quick_start::mojom::QuickStartDecoder> receiver_set_;
  mojo::SharedRemote<ash::quick_start::mojom::QuickStartDecoder> shared_remote_;
};

}  // namespace nearby
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_QUICK_START_DECODER_H_
