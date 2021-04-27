// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/rmad/fake_rmad_client.h"

namespace chromeos {

FakeRmadClient::FakeRmadClient() = default;
FakeRmadClient::~FakeRmadClient() = default;

void FakeRmadClient::GetCurrentState(
    DBusMethodCallback<rmad::GetStateReply> callback) {
  // TODO(gavindodd): Implement fake state.
  std::move(callback).Run(base::nullopt);
}

}  // namespace chromeos
