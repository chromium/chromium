// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_quick_start_decoder.h"

namespace ash {
namespace nearby {

MockQuickStartDecoder::~MockQuickStartDecoder() = default;

MockQuickStartDecoder::MockQuickStartDecoder() {
  mojo::PendingRemote<ash::quick_start::mojom::QuickStartDecoder>
      pending_remote;
  receiver_set_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  shared_remote_.Bind(std::move(pending_remote), /*bind_task_runner=*/nullptr);
}

void MockQuickStartDecoder::BindInterface(
    mojo::PendingReceiver<ash::quick_start::mojom::QuickStartDecoder>
        pending_receiver) {
  receiver_set_.Add(this, std::move(pending_receiver));
}

}  // namespace nearby
}  // namespace ash
