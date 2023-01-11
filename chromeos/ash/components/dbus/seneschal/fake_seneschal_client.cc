// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/seneschal/fake_seneschal_client.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

namespace {

FakeSeneschalClient* g_instance = nullptr;

}  // namespace

// static
FakeSeneschalClient* FakeSeneschalClient::Get() {
  return g_instance;
}

FakeSeneschalClient::FakeSeneschalClient() {
  DCHECK(!g_instance);
  g_instance = this;

  share_path_response_.set_success(true);
  share_path_response_.set_path("foo");
  unshare_path_response_.set_success(true);
}

FakeSeneschalClient::~FakeSeneschalClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void FakeSeneschalClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeSeneschalClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeSeneschalClient::NotifySeneschalStopped() {
  for (auto& observer : observer_list_) {
    observer.SeneschalServiceStopped();
  }
}
void FakeSeneschalClient::NotifySeneschalStarted() {
  for (auto& observer : observer_list_) {
    observer.SeneschalServiceStarted();
  }
}

void FakeSeneschalClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeSeneschalClient::SharePath(
    const vm_tools::seneschal::SharePathRequest& request,
    chromeos::DBusMethodCallback<vm_tools::seneschal::SharePathResponse>
        callback) {
  share_path_called_ = true;
  last_share_path_request_ = request;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), share_path_response_));
}

void FakeSeneschalClient::UnsharePath(
    const vm_tools::seneschal::UnsharePathRequest& request,
    chromeos::DBusMethodCallback<vm_tools::seneschal::UnsharePathResponse>
        callback) {
  unshare_path_called_ = true;
  last_unshare_path_request_ = request;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), unshare_path_response_));
}

}  // namespace ash
