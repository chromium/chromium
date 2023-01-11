// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/vm_plugin_dispatcher/fake_vm_plugin_dispatcher_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

FakeVmPluginDispatcherClient::FakeVmPluginDispatcherClient() = default;

FakeVmPluginDispatcherClient::~FakeVmPluginDispatcherClient() = default;

void FakeVmPluginDispatcherClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeVmPluginDispatcherClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeVmPluginDispatcherClient::StartVm(
    const vm_tools::plugin_dispatcher::StartVmRequest& request,
    chromeos::DBusMethodCallback<vm_tools::plugin_dispatcher::StartVmResponse>
        callback) {
  start_vm_called_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), start_vm_response_));
}

void FakeVmPluginDispatcherClient::ListVms(
    const vm_tools::plugin_dispatcher::ListVmRequest& request,
    chromeos::DBusMethodCallback<vm_tools::plugin_dispatcher::ListVmResponse>
        callback) {
  list_vms_called_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), list_vms_response_));
}

void FakeVmPluginDispatcherClient::StopVm(
    const vm_tools::plugin_dispatcher::StopVmRequest& request,
    chromeos::DBusMethodCallback<vm_tools::plugin_dispatcher::StopVmResponse>
        callback) {
  stop_vm_called_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                vm_tools::plugin_dispatcher::StopVmResponse()));
}

void FakeVmPluginDispatcherClient::SuspendVm(
    const vm_tools::plugin_dispatcher::SuspendVmRequest& request,
    chromeos::DBusMethodCallback<vm_tools::plugin_dispatcher::SuspendVmResponse>
        callback) {
  suspend_vm_called_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     vm_tools::plugin_dispatcher::SuspendVmResponse()));
}

void FakeVmPluginDispatcherClient::ShowVm(
    const vm_tools::plugin_dispatcher::ShowVmRequest& request,
    chromeos::DBusMethodCallback<vm_tools::plugin_dispatcher::ShowVmResponse>
        callback) {
  show_vm_called_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                vm_tools::plugin_dispatcher::ShowVmResponse()));
}

void FakeVmPluginDispatcherClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeVmPluginDispatcherClient::NotifyVmToolsStateChanged(
    const vm_tools::plugin_dispatcher::VmToolsStateChangedSignal& signal) {
  for (auto& observer : observer_list_) {
    observer.OnVmToolsStateChanged(signal);
  }
}

void FakeVmPluginDispatcherClient::NotifyVmStateChanged(
    const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal) {
  for (auto& observer : observer_list_) {
    observer.OnVmStateChanged(signal);
  }
}

}  // namespace ash
