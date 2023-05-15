// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_DECODER_NEARBY_DECODER_H_
#define CHROME_SERVICES_SHARING_NEARBY_DECODER_NEARBY_DECODER_H_

#include <vector>

#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace sharing {

class NearbySharingDecoder : public mojom::NearbySharingDecoder {
 public:
  NearbySharingDecoder(
      mojo::PendingReceiver<mojom::NearbySharingDecoder> receiver,
      base::OnceClosure on_disconnect);
  NearbySharingDecoder(const NearbySharingDecoder&) = delete;
  NearbySharingDecoder& operator=(const NearbySharingDecoder&) = delete;
  ~NearbySharingDecoder() override;

  // mojom::NearbySharingDecoder:
  void DecodeAdvertisement(const std::vector<uint8_t>& data,
                           DecodeAdvertisementCallback callback) override;
  void DecodeFrame(const std::vector<uint8_t>& data,
                   DecodeFrameCallback callback) override;

 private:
  mojo::Receiver<mojom::NearbySharingDecoder> receiver_;
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_NEARBY_DECODER_NEARBY_DECODER_H_
