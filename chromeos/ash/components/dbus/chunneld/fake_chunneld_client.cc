// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/chunneld/fake_chunneld_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace ash
