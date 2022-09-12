// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/fake_arc_keymaster_client.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"

namespace ash {

void FakeArcKeymasterClient::Init(dbus::Bus* bus) {}

void FakeArcKeymasterClient::BootstrapMojoConnection(
    base::ScopedFD fd,
    chromeos::VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false));
}

}  // namespace ash
