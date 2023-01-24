// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/fake_arc_keymint_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

void FakeArcKeyMintClient::Init(dbus::Bus* bus) {}

void FakeArcKeyMintClient::BootstrapMojoConnection(
    base::ScopedFD fd,
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false));
}

}  // namespace ash
