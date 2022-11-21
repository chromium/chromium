// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/fake_arc_sensor_service_client.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

namespace {

// Used to track the fake instance, mirrors the instance in the base class.
FakeArcSensorServiceClient* g_instance = nullptr;

}  // namespace

FakeArcSensorServiceClient::FakeArcSensorServiceClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeArcSensorServiceClient::~FakeArcSensorServiceClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeArcSensorServiceClient* FakeArcSensorServiceClient::Get() {
  return g_instance;
}

void FakeArcSensorServiceClient::BootstrapMojoConnection(
    int fd,
    const std::string& token,
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace ash
