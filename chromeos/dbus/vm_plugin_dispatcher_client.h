// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_VM_PLUGIN_DISPATCHER_CLIENT_H_
#define CHROMEOS_DBUS_VM_PLUGIN_DISPATCHER_CLIENT_H_

#include <memory>

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher.pb.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// VmPluginDispatcherClient is used to communicate with the Plugin VM
// Dispatcher, which manages plugin VMs.
class COMPONENT_EXPORT(CHROMEOS_DBUS) VmPluginDispatcherClient
    : public DBusClient {
 public:
  // Used to observe changes to VM state.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnVmStateChanged(
        const vm_tools::plugin_dispatcher::VmStateChangedSignal& signal) = 0;

   protected:
    ~Observer() override = default;
  };

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Asynchronously starts a given VM.
  virtual void StartVm(
      const vm_tools::plugin_dispatcher::StartVmRequest& request,
      DBusMethodCallback<vm_tools::plugin_dispatcher::StartVmResponse>
          callback) = 0;

  // Retrieve metadata about a specific or all VMs.
  virtual void ListVms(
      const vm_tools::plugin_dispatcher::ListVmRequest& request,
      DBusMethodCallback<vm_tools::plugin_dispatcher::ListVmResponse>
          callback) = 0;

  // Asynchronously stop a given VM. This does not close the UI.
  virtual void StopVm(
      const vm_tools::plugin_dispatcher::StopVmRequest& request,
      DBusMethodCallback<vm_tools::plugin_dispatcher::StopVmResponse>
          callback) = 0;

  // Asynchronously suspend a given VM. This does not close the UI.
  virtual void SuspendVm(
      const vm_tools::plugin_dispatcher::SuspendVmRequest& request,
      DBusMethodCallback<vm_tools::plugin_dispatcher::SuspendVmResponse>
          callback) = 0;

  // Start the UI component responsible for rendering VM display.
  virtual void ShowVm(
      const vm_tools::plugin_dispatcher::ShowVmRequest& request,
      DBusMethodCallback<vm_tools::plugin_dispatcher::ShowVmResponse>
          callback) = 0;

  // Runs |callback| when the VM Plugin Dispatcher service becomes available.
  // If already available, it is run async.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

  // Creates an instance of VmPluginDispatcherClient.
  static std::unique_ptr<VmPluginDispatcherClient> Create();

  ~VmPluginDispatcherClient() override;

 protected:
  // Create() should be used instead.
  VmPluginDispatcherClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(VmPluginDispatcherClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_VM_PLUGIN_DISPATCHER_CLIENT_H_
