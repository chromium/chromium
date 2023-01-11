// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/arc/fake_arc_data_snapshotd_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

FakeArcDataSnapshotdClient::FakeArcDataSnapshotdClient() = default;
FakeArcDataSnapshotdClient::~FakeArcDataSnapshotdClient() = default;

void FakeArcDataSnapshotdClient::Init(dbus::Bus* bus) {}

void FakeArcDataSnapshotdClient::GenerateKeyPair(
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeArcDataSnapshotdClient::ClearSnapshot(
    bool last,
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeArcDataSnapshotdClient::TakeSnapshot(
    const std::string& account_id,
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeArcDataSnapshotdClient::LoadSnapshot(
    const std::string& account_id,
    LoadSnapshotMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true, true));
}

void FakeArcDataSnapshotdClient::Update(
    int percent,
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeArcDataSnapshotdClient::ConnectToUiCancelledSignal(
    base::RepeatingClosure signal_callback,
    base::OnceCallback<void(bool)> on_connected_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(on_connected_callback), is_available_));
  signal_callback_ = std::move(signal_callback);
}

void FakeArcDataSnapshotdClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), is_available_));
}

}  // namespace ash
