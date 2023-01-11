// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/fake_arc_appfuse_provider_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

FakeArcAppfuseProviderClient::FakeArcAppfuseProviderClient() = default;

FakeArcAppfuseProviderClient::~FakeArcAppfuseProviderClient() = default;

void FakeArcAppfuseProviderClient::Init(dbus::Bus* bus) {}

void FakeArcAppfuseProviderClient::Mount(
    uint32_t uid,
    int32_t mount_id,
    chromeos::DBusMethodCallback<base::ScopedFD> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::ScopedFD()));
}

void FakeArcAppfuseProviderClient::Unmount(
    uint32_t uid,
    int32_t mount_id,
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false));
}

void FakeArcAppfuseProviderClient::OpenFile(
    uint32_t uid,
    int32_t mount_id,
    int32_t file_id,
    int32_t flags,
    chromeos::DBusMethodCallback<base::ScopedFD> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), base::ScopedFD()));
}

}  // namespace ash
