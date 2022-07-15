// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/fake_arc_data_snapshotd_client.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace ash {

FakeArcDataSnapshotdClient::FakeArcDataSnapshotdClient() = default;
FakeArcDataSnapshotdClient::~FakeArcDataSnapshotdClient() = default;

void FakeArcDataSnapshotdClient::Init(dbus::Bus* bus) {}

void FakeArcDataSnapshotdClient::GenerateKeyPair(
    VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeArcDataSnapshotdClient::ClearSnapshot(
    bool last,
    VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeArcDataSnapshotdClient::TakeSnapshot(const std::string& account_id,
                                              VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeArcDataSnapshotdClient::LoadSnapshot(
    const std::string& account_id,
    LoadSnapshotMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true, true));
}

void FakeArcDataSnapshotdClient::Update(int percent,
                                        VoidDBusMethodCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeArcDataSnapshotdClient::ConnectToUiCancelledSignal(
    base::RepeatingClosure signal_callback,
    base::OnceCallback<void(bool)> on_connected_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(on_connected_callback), is_available_));
  signal_callback_ = std::move(signal_callback);
}

void FakeArcDataSnapshotdClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), is_available_));
}

}  // namespace ash
