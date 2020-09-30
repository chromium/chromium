// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CONCIERGE_CLIENT_H_
#define CHROMEOS_DBUS_CONCIERGE_CLIENT_H_

#include <memory>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/observer_list.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// ConciergeClient is used to communicate with Concierge, which is used to
// start and stop VMs, as well as for disk image management.
class COMPONENT_EXPORT(CHROMEOS_DBUS) ConciergeClient : public DBusClient {
 public:
  // Used for observing Concierge service itself.
  class Observer : public base::CheckedObserver {
   public:
    // Called when Concierge service exits.
    virtual void ConciergeServiceStopped() = 0;
    // Called when Concierge service is either started or restarted.
    virtual void ConciergeServiceStarted() = 0;
  };

  // Used for observing VMs starting and stopping.
  class VmObserver {
   public:
    // OnVmStarted is signaled by Concierge when a VM starts.
    virtual void OnVmStarted(
        const vm_tools::concierge::VmStartedSignal& signal) = 0;

    // OnVmStopped is signaled by Concierge when a VM stops.
    virtual void OnVmStopped(
        const vm_tools::concierge::VmStoppedSignal& signal) = 0;

   protected:
    virtual ~VmObserver() = default;
  };

  // Used for observing all concierge signals related to running
  // containers (e.g. startup).
  class ContainerObserver {
   public:
    // OnContainerStartupFailed is signaled by Concierge after the long-running
    // container startup process's failure is detected. Note the signal protocol
    // buffer type is the same as in OnContainerStarted.
    virtual void OnContainerStartupFailed(
        const vm_tools::concierge::ContainerStartedSignal& signal) = 0;

   protected:
    virtual ~ContainerObserver() = default;
  };

  // Used for observing all concierge signals related to VM disk image
  // operations, e.g. importing.
  class DiskImageObserver {
   public:
    // OnDiskImageProgress is signaled by Concierge after an ImportDiskImage
    // call has been made and an update about the status of the import
    // is available.
    virtual void OnDiskImageProgress(
        const vm_tools::concierge::DiskImageStatusResponse& signal) = 0;

   protected:
    virtual ~DiskImageObserver() = default;
  };

  // Adds an observer for monitoring Concierge service.
  virtual void AddObserver(Observer* observer) = 0;
  // Removes an observer if added.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Adds an observer for VM start and stop.
  virtual void AddVmObserver(VmObserver* observer) = 0;
  // Removes an observer if added.
  virtual void RemoveVmObserver(VmObserver* observer) = 0;

  // Adds an observer for container startup.
  virtual void AddContainerObserver(ContainerObserver* observer) = 0;
  // Removes an observer if added.
  virtual void RemoveContainerObserver(ContainerObserver* observer) = 0;

  // Adds an observer for disk image operations.
  virtual void AddDiskImageObserver(DiskImageObserver* observer) = 0;
  // Adds an observer for disk image operations.
  virtual void RemoveDiskImageObserver(DiskImageObserver* observer) = 0;

  // IsVmSartedSignalConnected and IsVmStoppedSignalConnected must return true
  // before RestartCrostini is called.
  virtual bool IsVmStartedSignalConnected() = 0;
  virtual bool IsVmStoppedSignalConnected() = 0;

  // IsContainerStartupFailedSignalConnected must return true before
  // StartContainer is called.
  virtual bool IsContainerStartupFailedSignalConnected() = 0;

  // IsDiskImageProgressSignalConnected must return true before
  // ImportDiskImage is called.
  virtual bool IsDiskImageProgressSignalConnected() = 0;

  // Creates a disk image for the Termina VM.
  // |callback| is called after the method call finishes.
  virtual void CreateDiskImage(
      const vm_tools::concierge::CreateDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse>
          callback) = 0;

  // Creates a disk image for a VM.
  // |fd| references the source media (ISO).
  // |callback| is called after the method call finishes.
  virtual void CreateDiskImageWithFd(
      base::ScopedFD fd,
      const vm_tools::concierge::CreateDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse>
          callback) = 0;

  // Destroys a VM and removes its disk image.
  // |callback| is called after the method call finishes.
  virtual void DestroyDiskImage(
      const vm_tools::concierge::DestroyDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::DestroyDiskImageResponse>
          callback) = 0;

  // Imports a VM disk image.
  // |callback| is called after the method call finishes.
  virtual void ImportDiskImage(
      base::ScopedFD fd,
      const vm_tools::concierge::ImportDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::ImportDiskImageResponse>
          callback) = 0;

  // Cancels a VM disk image operation (import or export) that is being
  // executed.
  // |callback| is called after the method call finishes.
  virtual void CancelDiskImageOperation(
      const vm_tools::concierge::CancelDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::CancelDiskImageResponse>
          callback) = 0;

  // Retrieves the status of a disk image operation
  // |callback| is called after the method call finishes.
  virtual void DiskImageStatus(
      const vm_tools::concierge::DiskImageStatusRequest& request,
      DBusMethodCallback<vm_tools::concierge::DiskImageStatusResponse>
          callback) = 0;

  // Lists the Termina VMs.
  // |callback| is called after the method call finishes.
  virtual void ListVmDisks(
      const vm_tools::concierge::ListVmDisksRequest& request,
      DBusMethodCallback<vm_tools::concierge::ListVmDisksResponse>
          callback) = 0;

  // Starts a Termina VM if there is not alread one running.
  // |callback| is called after the method call finishes.
  virtual void StartTerminaVm(
      const vm_tools::concierge::StartVmRequest& request,
      DBusMethodCallback<vm_tools::concierge::StartVmResponse> callback) = 0;

  // Stops the named Termina VM if it is running.
  // |callback| is called after the method call finishes.
  virtual void StopVm(
      const vm_tools::concierge::StopVmRequest& request,
      DBusMethodCallback<vm_tools::concierge::StopVmResponse> callback) = 0;

  // Suspends the named Termina VM if it is running.
  // |callback| is called after the method call finishes.
  virtual void SuspendVm(
      const vm_tools::concierge::SuspendVmRequest& request,
      DBusMethodCallback<vm_tools::concierge::SuspendVmResponse> callback) = 0;

  // Resumes the named Termina VM if it is running.
  // |callback| is called after the method call finishes.
  virtual void ResumeVm(
      const vm_tools::concierge::ResumeVmRequest& request,
      DBusMethodCallback<vm_tools::concierge::ResumeVmResponse> callback) = 0;

  // Get VM Info.
  // |callback| is called after the method call finishes.
  virtual void GetVmInfo(
      const vm_tools::concierge::GetVmInfoRequest& request,
      DBusMethodCallback<vm_tools::concierge::GetVmInfoResponse> callback) = 0;

  // Get enterprise-reporting specific VM info.
  // |callback| is called after the method call finishes.
  virtual void GetVmEnterpriseReportingInfo(
      const vm_tools::concierge::GetVmEnterpriseReportingInfoRequest& request,
      DBusMethodCallback<
          vm_tools::concierge::GetVmEnterpriseReportingInfoResponse>
          callback) = 0;

  // Set VM's CPU restriction state.
  // |callback| is called after the method call finishes.
  virtual void SetVmCpuRestriction(
      const vm_tools::concierge::SetVmCpuRestrictionRequest& request,
      DBusMethodCallback<vm_tools::concierge::SetVmCpuRestrictionResponse>
          callback) = 0;

  // Registers |callback| to run when the Concierge service becomes available.
  // If the service is already available, or if connecting to the name-owner-
  // changed signal fails, |callback| will be run once asynchronously.
  // Otherwise, |callback| will be run once in the future after the service
  // becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

  // Gets SSH server public key of container and trusted SSH client private key
  // which can be used to connect to the container.
  // |callback| is called after the method call finishes.
  virtual void GetContainerSshKeys(
      const vm_tools::concierge::ContainerSshKeysRequest& request,
      DBusMethodCallback<vm_tools::concierge::ContainerSshKeysResponse>
          callback) = 0;

  // Attaches a USB device to a VM.
  // |callback| is called once the method call has finished.
  virtual void AttachUsbDevice(
      base::ScopedFD fd,
      const vm_tools::concierge::AttachUsbDeviceRequest& request,
      DBusMethodCallback<vm_tools::concierge::AttachUsbDeviceResponse>
          callback) = 0;

  // Removes a USB device from a VM it's been attached to.
  // |callback| is called once the method call has finished.
  virtual void DetachUsbDevice(
      const vm_tools::concierge::DetachUsbDeviceRequest& request,
      DBusMethodCallback<vm_tools::concierge::DetachUsbDeviceResponse>
          callback) = 0;

  // Starts ARCVM if there is not already one running.
  // |callback| is called after the method call finishes.
  virtual void StartArcVm(
      const vm_tools::concierge::StartArcVmRequest& request,
      DBusMethodCallback<vm_tools::concierge::StartVmResponse> callback) = 0;

  // Launches a resize operation for the specified disk image.
  // |callback| is called after the method call finishes, then you must use
  // |DiskImageStatus| to poll for task completion.
  virtual void ResizeDiskImage(
      const vm_tools::concierge::ResizeDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::ResizeDiskImageResponse>
          callback) = 0;

  // Sets the cryptohome id of the given VM.
  // |callback| is called after the method call finishes.
  virtual void SetVmId(
      const vm_tools::concierge::SetVmIdRequest& request,
      DBusMethodCallback<vm_tools::concierge::SetVmIdResponse> callback) = 0;

  // Creates an instance of ConciergeClient.
  static std::unique_ptr<ConciergeClient> Create();

  ~ConciergeClient() override;

 protected:
  // Create() should be used instead.
  ConciergeClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ConciergeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CONCIERGE_CLIENT_H_
