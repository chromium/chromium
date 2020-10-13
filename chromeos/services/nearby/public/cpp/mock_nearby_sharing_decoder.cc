// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/nearby/public/cpp/mock_nearby_sharing_decoder.h"

namespace chromeos {
namespace nearby {

MockNearbySharingDecoder::MockNearbySharingDecoder() {
  mojo::PendingRemote<sharing::mojom::NearbySharingDecoder> pending_remote;
  receiver_.Bind(pending_remote.InitWithNewPipeAndPassReceiver());
  shared_remote_.Bind(std::move(pending_remote), /*bind_task_runner=*/nullptr);
}

MockNearbySharingDecoder::~MockNearbySharingDecoder() = default;

}  // namespace nearby
}  // namespace chromeos
