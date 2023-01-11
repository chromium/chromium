// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_key_entered_handler.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"

namespace ash::bluetooth_config {

FakeKeyEnteredHandler::FakeKeyEnteredHandler(
    mojo::PendingReceiver<mojom::KeyEnteredHandler> receiver) {
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &FakeKeyEnteredHandler::OnDisconnect, base::Unretained(this)));
}

FakeKeyEnteredHandler::~FakeKeyEnteredHandler() = default;

void FakeKeyEnteredHandler::DisconnectMojoPipe() {
  receiver_.reset();

  // Allow the disconnection to propagate.
  base::RunLoop().RunUntilIdle();
}

bool FakeKeyEnteredHandler::IsMojoPipeConnected() const {
  return receiver_.is_bound();
}

void FakeKeyEnteredHandler::HandleKeyEntered(uint8_t num_keys_entered) {
  num_keys_entered_ = num_keys_entered;
}

void FakeKeyEnteredHandler::OnDisconnect() {
  receiver_.reset();
}

}  // namespace ash::bluetooth_config
