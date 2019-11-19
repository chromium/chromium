// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_vm_plugin_dispatcher_client.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

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
    DBusMethodCallback<vm_tools::plugin_dispatcher::StartVmResponse> callback) {
  start_vm_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), start_vm_response_));
}

void FakeVmPluginDispatcherClient::ListVms(
    const vm_tools::plugin_dispatcher::ListVmRequest& request,
    DBusMethodCallback<vm_tools::plugin_dispatcher::ListVmResponse> callback) {
  list_vms_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), list_vms_response_));
}

void FakeVmPluginDispatcherClient::StopVm(
    const vm_tools::plugin_dispatcher::StopVmRequest& request,
    DBusMethodCallback<vm_tools::plugin_dispatcher::StopVmResponse> callback) {
  stop_vm_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                vm_tools::plugin_dispatcher::StopVmResponse()));
}

void FakeVmPluginDispatcherClient::SuspendVm(
    const vm_tools::plugin_dispatcher::SuspendVmRequest& request,
    DBusMethodCallback<vm_tools::plugin_dispatcher::SuspendVmResponse>
        callback) {
  suspend_vm_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     vm_tools::plugin_dispatcher::SuspendVmResponse()));
}

void FakeVmPluginDispatcherClient::ShowVm(
    const vm_tools::plugin_dispatcher::ShowVmRequest& request,
    DBusMethodCallback<vm_tools::plugin_dispatcher::ShowVmResponse> callback) {
  show_vm_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                vm_tools::plugin_dispatcher::ShowVmResponse()));
}

void FakeVmPluginDispatcherClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeVmPluginDispatcherClient::NotifyVmStateChanged(
    const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal) {
  for (auto& observer : observer_list_) {
    observer.OnVmStateChanged(signal);
  }
}

}  // namespace chromeos
