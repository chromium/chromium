// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_VM_PLUGIN_DISPATCHER_FAKE_VM_PLUGIN_DISPATCHER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_VM_PLUGIN_DISPATCHER_FAKE_VM_PLUGIN_DISPATCHER_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher_client.h"

namespace ash {

class COMPONENT_EXPORT(ASH_DBUS_VM_PLUGIN_DISPATCHER)
    FakeVmPluginDispatcherClient : public VmPluginDispatcherClient {
 public:
  FakeVmPluginDispatcherClient();

  FakeVmPluginDispatcherClient(const FakeVmPluginDispatcherClient&) = delete;
  FakeVmPluginDispatcherClient& operator=(const FakeVmPluginDispatcherClient&) =
      delete;

  ~FakeVmPluginDispatcherClient() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void StartVm(
      const vm_tools::plugin_dispatcher::StartVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::plugin_dispatcher::StartVmResponse>
          callback) override;

  void ListVms(
      const vm_tools::plugin_dispatcher::ListVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::plugin_dispatcher::ListVmResponse>
          callback) override;

  void StopVm(
      const vm_tools::plugin_dispatcher::StopVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::plugin_dispatcher::StopVmResponse>
          callback) override;

  void SuspendVm(
      const vm_tools::plugin_dispatcher::SuspendVmRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::plugin_dispatcher::SuspendVmResponse> callback) override;

  void ShowVm(
      const vm_tools::plugin_dispatcher::ShowVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::plugin_dispatcher::ShowVmResponse>
          callback) override;

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

  // Helper functions for testing interaction with the VmPluginDispatcherClient.
  bool start_vm_called() const { return start_vm_called_; }
  bool list_vms_called() const { return list_vms_called_; }
  bool stop_vm_called() const { return stop_vm_called_; }
  bool suspend_vm_called() const { return suspend_vm_called_; }
  bool show_vm_called() const { return show_vm_called_; }

  void set_start_vm_response(
      std::optional<vm_tools::plugin_dispatcher::StartVmResponse> response) {
    start_vm_response_ = response;
  }

  void set_list_vms_response(
      std::optional<vm_tools::plugin_dispatcher::ListVmResponse> response) {
    list_vms_response_ = response;
  }

  // Calls observers of the OnVmToolsStateChanged signal
  void NotifyVmToolsStateChanged(
      const vm_tools::plugin_dispatcher::VmToolsStateChangedSignal& signal);

  // Calls observers of the OnVmStateChanged signal
  void NotifyVmStateChanged(
      const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal);

 protected:
  void Init(dbus::Bus* bus) override {}

 private:
  bool start_vm_called_ = false;
  bool list_vms_called_ = false;
  bool stop_vm_called_ = false;
  bool suspend_vm_called_ = false;
  bool show_vm_called_ = false;

  std::optional<vm_tools::plugin_dispatcher::StartVmResponse>
      start_vm_response_;
  std::optional<vm_tools::plugin_dispatcher::ListVmResponse> list_vms_response_;

  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_VM_PLUGIN_DISPATCHER_FAKE_VM_PLUGIN_DISPATCHER_CLIENT_H_
