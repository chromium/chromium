// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_VM_PLUGIN_DISPATCHER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_VM_PLUGIN_DISPATCHER_CLIENT_H_

#include "base/observer_list.h"
#include "chromeos/dbus/vm_plugin_dispatcher_client.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeVmPluginDispatcherClient
    : public VmPluginDispatcherClient {
 public:
  FakeVmPluginDispatcherClient();
  ~FakeVmPluginDispatcherClient() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void StartVm(const vm_tools::plugin_dispatcher::StartVmRequest& request,
               DBusMethodCallback<vm_tools::plugin_dispatcher::StartVmResponse>
                   callback) override;

  void ListVms(const vm_tools::plugin_dispatcher::ListVmRequest& request,
               DBusMethodCallback<vm_tools::plugin_dispatcher::ListVmResponse>
                   callback) override;

  void StopVm(const vm_tools::plugin_dispatcher::StopVmRequest& request,
              DBusMethodCallback<vm_tools::plugin_dispatcher::StopVmResponse>
                  callback) override;

  void SuspendVm(
      const vm_tools::plugin_dispatcher::SuspendVmRequest& request,
      DBusMethodCallback<vm_tools::plugin_dispatcher::SuspendVmResponse>
          callback) override;

  void ShowVm(const vm_tools::plugin_dispatcher::ShowVmRequest& request,
              DBusMethodCallback<vm_tools::plugin_dispatcher::ShowVmResponse>
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
      const vm_tools::plugin_dispatcher::StartVmResponse& response) {
    start_vm_response_ = response;
  }

  void set_list_vms_response(
      const vm_tools::plugin_dispatcher::ListVmResponse& response) {
    list_vms_response_ = response;
  }

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

  vm_tools::plugin_dispatcher::StartVmResponse start_vm_response_;
  vm_tools::plugin_dispatcher::ListVmResponse list_vms_response_;

  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(FakeVmPluginDispatcherClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_VM_PLUGIN_DISPATCHER_CLIENT_H_
