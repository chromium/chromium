// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/chunneld/fake_chunneld_client.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace ash {

FakeChunneldClient::FakeChunneldClient() {}

FakeChunneldClient::~FakeChunneldClient() = default;

void FakeChunneldClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeChunneldClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeChunneldClient::NotifyChunneldStopped() {
  for (auto& observer : observer_list_) {
    observer.ChunneldServiceStopped();
  }
}
void FakeChunneldClient::NotifyChunneldStarted() {
  for (auto& observer : observer_list_) {
    observer.ChunneldServiceStarted();
  }
}

void FakeChunneldClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace ash
