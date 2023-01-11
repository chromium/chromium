// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/chromebox_for_meetings/fake_cfm_hotline_client.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

FakeCfmHotlineClient::FakeCfmHotlineClient() = default;
FakeCfmHotlineClient::~FakeCfmHotlineClient() = default;

void FakeCfmHotlineClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeCfmHotlineClient::BootstrapMojoConnection(
    base::ScopedFD fd,
    BootstrapMojoConnectionCallback result_callback) {
  // Fake that the mojo connection has been successfully established.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_callback), true));
}

void FakeCfmHotlineClient::AddObserver(cfm::CfmObserver* observer) {
  observer_list_.AddObserver(observer);
}

void FakeCfmHotlineClient::RemoveObserver(cfm::CfmObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool FakeCfmHotlineClient::FakeEmitSignal(const std::string& interface_name) {
  for (auto& observer : observer_list_) {
    if (observer.ServiceRequestReceived(interface_name)) {
      return true;
    }
  }
  return false;
}

}  // namespace ash
