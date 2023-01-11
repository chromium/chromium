// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/cdm_factory_daemon/fake_cdm_factory_daemon_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace ash {

FakeCdmFactoryDaemonClient::FakeCdmFactoryDaemonClient() = default;
FakeCdmFactoryDaemonClient::~FakeCdmFactoryDaemonClient() = default;

void FakeCdmFactoryDaemonClient::BootstrapMojoConnection(
    base::ScopedFD fd,
    base::OnceCallback<void(bool success)> callback) {
  const bool success = true;
  std::move(callback).Run(success);
}

void FakeCdmFactoryDaemonClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  std::move(callback).Run(true);
}

}  // namespace ash
