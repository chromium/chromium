// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/arc/fake_arc_appfuse_provider_client.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeArcAppfuseProviderClient::FakeArcAppfuseProviderClient() = default;

FakeArcAppfuseProviderClient::~FakeArcAppfuseProviderClient() = default;

void FakeArcAppfuseProviderClient::Init(dbus::Bus* bus) {}

void FakeArcAppfuseProviderClient::Mount(
    uint32_t uid,
    int32_t mount_id,
    DBusMethodCallback<base::ScopedFD> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::ScopedFD()));
}

void FakeArcAppfuseProviderClient::Unmount(uint32_t uid,
                                           int32_t mount_id,
                                           VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false));
}

void FakeArcAppfuseProviderClient::OpenFile(
    uint32_t uid,
    int32_t mount_id,
    int32_t file_id,
    int32_t flags,
    DBusMethodCallback<base::ScopedFD> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::ScopedFD()));
}

}  // namespace chromeos
