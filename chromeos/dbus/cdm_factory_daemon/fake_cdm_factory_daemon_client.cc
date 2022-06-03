// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cdm_factory_daemon/fake_cdm_factory_daemon_client.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeCdmFactoryDaemonClient::FakeCdmFactoryDaemonClient() = default;
FakeCdmFactoryDaemonClient::~FakeCdmFactoryDaemonClient() = default;

void FakeCdmFactoryDaemonClient::BootstrapMojoConnection(
    base::ScopedFD fd,
    base::OnceCallback<void(bool success)> callback) {
  const bool success = true;
  std::move(callback).Run(success);
}

}  // namespace chromeos
