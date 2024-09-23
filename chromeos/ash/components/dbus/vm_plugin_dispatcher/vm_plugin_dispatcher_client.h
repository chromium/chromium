// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_VM_PLUGIN_DISPATCHER_VM_PLUGIN_DISPATCHER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_VM_PLUGIN_DISPATCHER_VM_PLUGIN_DISPATCHER_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "dbus/object_proxy.h"

namespace ash {

// VmPluginDispatcherClient is used to communicate with the Plugin VM
// Dispatcher, which manages plugin VMs.
class COMPONENT_EXPORT(ASH_DBUS_VM_PLUGIN_DISPATCHER) VmPluginDispatcherClient
    : public chromeos::DBusClient {
 public:
  // Used to observe changes to VM tool's state and VM's state.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnVmToolsStateChanged(
        const vm_tools::plugin_dispatcher::VmToolsStateChangedSignal&
            signal) = 0;

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
      chromeos::DBusMethodCallback<vm_tools::plugin_dispatcher::StartVmResponse>
          callback) = 0;

  // Retrieve metadata about a specific or all VMs.
  virtual void ListVms(
      const vm_tools::plugin_dispatcher::ListVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::plugin_dispatcher::ListVmResponse>
          callback) = 0;

  // Asynchronously stop a given VM. This does not close the UI.
  virtual void StopVm(
      const vm_tools::plugin_dispatcher::StopVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::plugin_dispatcher::StopVmResponse>
          callback) = 0;

  // Asynchronously suspend a given VM. This does not close the UI.
  virtual void SuspendVm(
      const vm_tools::plugin_dispatcher::SuspendVmRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::plugin_dispatcher::SuspendVmResponse> callback) = 0;

  // Start the UI component responsible for rendering VM display.
  virtual void ShowVm(
      const vm_tools::plugin_dispatcher::ShowVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::plugin_dispatcher::ShowVmResponse>
          callback) = 0;

  // Runs |callback| when the VM Plugin Dispatcher service becomes available.
  // If already available, it is run async.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static VmPluginDispatcherClient* Get();

  VmPluginDispatcherClient(const VmPluginDispatcherClient&) = delete;
  VmPluginDispatcherClient& operator=(const VmPluginDispatcherClient&) = delete;

 protected:
  // Initialize() should be used instead.
  VmPluginDispatcherClient();

  ~VmPluginDispatcherClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_VM_PLUGIN_DISPATCHER_VM_PLUGIN_DISPATCHER_CLIENT_H_
