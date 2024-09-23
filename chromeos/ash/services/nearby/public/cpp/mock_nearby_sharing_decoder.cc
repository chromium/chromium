// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_sharing_decoder.h"

namespace ash {
namespace nearby {

MockNearbySharingDecoder::MockNearbySharingDecoder() {
  mojo::PendingRemote<::sharing::mojom::NearbySharingDecoder> pending_remote;
  receiver_set_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  shared_remote_.Bind(std::move(pending_remote), /*bind_task_runner=*/nullptr);
}

MockNearbySharingDecoder::~MockNearbySharingDecoder() = default;

void MockNearbySharingDecoder::BindInterface(
    mojo::PendingReceiver<::sharing::mojom::NearbySharingDecoder>
        pending_receiver) {
  receiver_set_.Add(this, std::move(pending_receiver));
}

}  // namespace nearby
}  // namespace ash
