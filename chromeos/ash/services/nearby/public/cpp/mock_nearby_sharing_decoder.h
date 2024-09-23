// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_NEARBY_SHARING_DECODER_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_NEARBY_SHARING_DECODER_H_

#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace nearby {

class MockNearbySharingDecoder : public ::sharing::mojom::NearbySharingDecoder {
 public:
  MockNearbySharingDecoder();
  explicit MockNearbySharingDecoder(const MockNearbySharingDecoder&) = delete;
  MockNearbySharingDecoder& operator=(const MockNearbySharingDecoder&) = delete;
  ~MockNearbySharingDecoder() override;

  const mojo::SharedRemote<::sharing::mojom::NearbySharingDecoder>&
  shared_remote() const {
    return shared_remote_;
  }

  void BindInterface(
      mojo::PendingReceiver<::sharing::mojom::NearbySharingDecoder>
          pending_receiver);

  // ::sharing::mojom::NearbySharingDecoder:
  MOCK_METHOD(void,
              DecodeAdvertisement,
              (const std::vector<uint8_t>& data,
               DecodeAdvertisementCallback callback),
              (override));
  MOCK_METHOD(void,
              DecodeFrame,
              (const std::vector<uint8_t>& data, DecodeFrameCallback callback),
              (override));

 private:
  mojo::ReceiverSet<::sharing::mojom::NearbySharingDecoder> receiver_set_;
  mojo::SharedRemote<::sharing::mojom::NearbySharingDecoder> shared_remote_;
};

}  // namespace nearby
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_MOCK_NEARBY_SHARING_DECODER_H_
