// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/test_support/libassistant_media_controller_mock.h"

namespace chromeos {
namespace assistant {

LibassistantMediaControllerMock::LibassistantMediaControllerMock() = default;
LibassistantMediaControllerMock::~LibassistantMediaControllerMock() = default;

void LibassistantMediaControllerMock::Bind(
    mojo::PendingReceiver<chromeos::libassistant::mojom::MediaController>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void LibassistantMediaControllerMock::FlushForTesting() {
  receiver_.FlushForTesting();
}

}  // namespace assistant
}  // namespace chromeos
