// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/fake_arc_camera_client.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

namespace {

// Used to track the fake instance, mirrors the instance in the base class.
FakeArcCameraClient* g_instance = nullptr;

}  // namespace

FakeArcCameraClient::FakeArcCameraClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeArcCameraClient::~FakeArcCameraClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeArcCameraClient* FakeArcCameraClient::Get() {
  return g_instance;
}

void FakeArcCameraClient::StartService(
    int fd,
    const std::string& token,
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace ash
