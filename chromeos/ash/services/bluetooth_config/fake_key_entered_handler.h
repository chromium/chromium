// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_KEY_ENTERED_HANDLER_H_
#define CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_KEY_ENTERED_HANDLER_H_

#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::bluetooth_config {

class FakeKeyEnteredHandler : public mojom::KeyEnteredHandler {
 public:
  FakeKeyEnteredHandler(
      mojo::PendingReceiver<mojom::KeyEnteredHandler> receiver);
  ~FakeKeyEnteredHandler() override;

  void DisconnectMojoPipe();

  bool IsMojoPipeConnected() const;

  uint8_t num_keys_entered() const { return num_keys_entered_; }

 private:
  // mojom::KeyEnteredHandler:
  void HandleKeyEntered(uint8_t num_keys_entered) override;

  void OnDisconnect();

  uint8_t num_keys_entered_ = 0;

  mojo::Receiver<mojom::KeyEnteredHandler> receiver_{this};
};

}  // namespace ash::bluetooth_config

#endif  // CHROMEOS_ASH_SERVICES_BLUETOOTH_CONFIG_FAKE_KEY_ENTERED_HANDLER_H_
