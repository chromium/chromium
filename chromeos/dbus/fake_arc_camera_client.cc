// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_arc_camera_client.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

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

void FakeArcCameraClient::StartService(int fd,
                                       const std::string& token,
                                       VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace chromeos
