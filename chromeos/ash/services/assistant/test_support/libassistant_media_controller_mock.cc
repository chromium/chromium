// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/test_support/libassistant_media_controller_mock.h"

namespace ash::assistant {

LibassistantMediaControllerMock::LibassistantMediaControllerMock() = default;
LibassistantMediaControllerMock::~LibassistantMediaControllerMock() = default;

void LibassistantMediaControllerMock::Bind(
    mojo::PendingReceiver<libassistant::mojom::MediaController>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void LibassistantMediaControllerMock::FlushForTesting() {
  receiver_.FlushForTesting();
}

}  // namespace ash::assistant
