// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CONCIERGE_CONCIERGE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CONCIERGE_CONCIERGE_CLIENT_H_

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "dbus/object_proxy.h"

namespace ash {

class FakeCiceroneClient;

// ConciergeClient is used to communicate with Concierge, which is used to
// start and stop VMs, as well as for disk image management.
class COMPONENT_EXPORT(CONCIERGE) ConciergeClient
    : public chromeos::DBusClient {
 public:
  static constexpr base::ObserverListPolicy kObserverListPolicy =
      base::ObserverListPolicy::EXISTING_ONLY;

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
        const vm_tools::concierge::VmStartedSignal& signal) {}

    // OnVmStopped is signaled by Concierge when a VM stops.
    virtual void OnVmStopped(
        const vm_tools::concierge::VmStoppedSignal& signal) {}

    // OnVmStopping is signaled by Concierge when a VM is stopping.
    virtual void OnVmStopping(
        const vm_tools::concierge::VmStoppingSignal& signal) {}

    // OnVmSwapping is signaled by Concierge when a VM is swapping.
    virtual void OnVmSwapping(
        const vm_tools::concierge::VmSwappingSignal& signal) {}

   protected:
    virtual ~VmObserver() = default;
  };

  // Used for observing all concierge signals related to VM disk image
  // operations, e.g. importing.
  class DiskImageObserver {
   public:
    // OnDiskImageProgress is signaled by Concierge after an
    // {Import,Export}DiskImage call has been made and an update about the
    // status of the import/export is available.
    virtual void OnDiskImageProgress(
        const vm_tools::concierge::DiskImageStatusResponse& signal) = 0;

   protected:
    virtual ~DiskImageObserver() = default;
  };

  ConciergeClient(const ConciergeClient&) = delete;
  ConciergeClient& operator=(const ConciergeClient&) = delete;

  // Adds an observer for monitoring Concierge service.
  virtual void AddObserver(Observer* observer) = 0;
  // Removes an observer if added.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Adds an observer for VM start and stop.
  virtual void AddVmObserver(VmObserver* observer) = 0;
  // Removes an observer if added.
  virtual void RemoveVmObserver(VmObserver* observer) = 0;

  // Adds an observer for disk image operations.
  virtual void AddDiskImageObserver(DiskImageObserver* observer) = 0;
  // Adds an observer for disk image operations.
  virtual void RemoveDiskImageObserver(DiskImageObserver* observer) = 0;

  // IsVmStartedSignalConnected and IsVmStoppedSignalConnected must return true
  // before RestartCrostini is called.
  virtual bool IsVmStartedSignalConnected() = 0;
  virtual bool IsVmStoppedSignalConnected() = 0;
  virtual bool IsVmStoppingSignalConnected() = 0;

  // IsDiskImageProgressSignalConnected must return true before
  // ImportDiskImage is called.
  virtual bool IsDiskImageProgressSignalConnected() = 0;

  // Creates a disk image for the Termina VM.
  // |callback| is called after the method call finishes.
  virtual void CreateDiskImage(
      const vm_tools::concierge::CreateDiskImageRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse>
          callback) = 0;

  // Creates a disk image for a VM.
  // |fd| references the source media (ISO).
  // |callback| is called after the method call finishes.
  virtual void CreateDiskImageWithFd(
      base::ScopedFD fd,
      const vm_tools::concierge::CreateDiskImageRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse>
          callback) = 0;

  // Destroys a VM and removes its disk image.
  // |callback| is called after the method call finishes.
  virtual void DestroyDiskImage(
      const vm_tools::concierge::DestroyDiskImageRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::DestroyDiskImageResponse> callback) = 0;

  // Imports a VM disk image.
  // |callback| is called after the method call finishes.
  virtual void ImportDiskImage(
      base::ScopedFD fd,
      const vm_tools::concierge::ImportDiskImageRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ImportDiskImageResponse>
          callback) = 0;

  // Exports a VM disk image.
  // |callback| is called after the method call finishes.
  virtual void ExportDiskImage(
      std::vector<base::ScopedFD> fds,
      const vm_tools::concierge::ExportDiskImageRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ExportDiskImageResponse>
          callback) = 0;

  // Cancels a VM disk image operation (import or export) that is being
  // executed.
  // |callback| is called after the method call finishes.
  virtual void CancelDiskImageOperation(
      const vm_tools::concierge::CancelDiskImageRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::CancelDiskImageResponse>
          callback) = 0;

  // Retrieves the status of a disk image operation
  // |callback| is called after the method call finishes.
  virtual void DiskImageStatus(
      const vm_tools::concierge::DiskImageStatusRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::DiskImageStatusResponse>
          callback) = 0;

  // Lists the Termina VMs.
  // |callback| is called after the method call finishes.
  virtual void ListVmDisks(
      const vm_tools::concierge::ListVmDisksRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ListVmDisksResponse>
          callback) = 0;

  // Starts a Termina VM if there is not already one running.
  // |callback| is called after the method call finishes.
  virtual void StartVm(
      const vm_tools::concierge::StartVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
          callback) = 0;

  // Starts a Termina VM if there is not already one running.
  // |fds| references an extra image for concierge to use.
  // |callback| is called after the method call finishes.
  virtual void StartVmWithFd(
      base::ScopedFD fd,
      const vm_tools::concierge::StartVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
          callback) = 0;

  // Stops the named Termina VM if it is running.
  // |callback| is called after the method call finishes.
  virtual void StopVm(
      const vm_tools::concierge::StopVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::StopVmResponse>
          callback) = 0;

  // Suspends the named Termina VM if it is running.
  // |callback| is called after the method call finishes.
  virtual void SuspendVm(
      const vm_tools::concierge::SuspendVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::SuspendVmResponse>
          callback) = 0;

  // Resumes the named Termina VM if it is running.
  // |callback| is called after the method call finishes.
  virtual void ResumeVm(
      const vm_tools::concierge::ResumeVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ResumeVmResponse>
          callback) = 0;

  // Get VM Info.
  // |callback| is called after the method call finishes.
  virtual void GetVmInfo(
      const vm_tools::concierge::GetVmInfoRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::GetVmInfoResponse>
          callback) = 0;

  // Get enterprise-reporting specific VM info.
  // |callback| is called after the method call finishes.
  virtual void GetVmEnterpriseReportingInfo(
      const vm_tools::concierge::GetVmEnterpriseReportingInfoRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::GetVmEnterpriseReportingInfoResponse>
          callback) = 0;

  // Performs necessary operations to complete the boot of ARCVM.
  // |callback| is called after the method call finishes.
  virtual void ArcVmCompleteBoot(
      const vm_tools::concierge::ArcVmCompleteBootRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::ArcVmCompleteBootResponse> callback) = 0;

  // Set VM's CPU restriction state.
  // |callback| is called after the method call finishes.
  virtual void SetVmCpuRestriction(
      const vm_tools::concierge::SetVmCpuRestrictionRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::SetVmCpuRestrictionResponse> callback) = 0;

  // Registers |callback| to run when the Concierge service becomes available.
  // If the service is already available, or if connecting to the name-owner-
  // changed signal fails, |callback| will be run once asynchronously.
  // Otherwise, |callback| will be run once in the future after the service
  // becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

  // Attaches a USB device to a VM.
  // |callback| is called once the method call has finished.
  virtual void AttachUsbDevice(
      base::ScopedFD fd,
      const vm_tools::concierge::AttachUsbDeviceRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::AttachUsbDeviceResponse>
          callback) = 0;

  // Removes a USB device from a VM it's been attached to.
  // |callback| is called once the method call has finished.
  virtual void DetachUsbDevice(
      const vm_tools::concierge::DetachUsbDeviceRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::DetachUsbDeviceResponse>
          callback) = 0;

  // Starts ARCVM if there is not already one running.
  // |callback| is called after the method call finishes.
  virtual void StartArcVm(
      const vm_tools::concierge::StartArcVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
          callback) = 0;

  // Launches a resize operation for the specified disk image.
  // |callback| is called after the method call finishes, then you must use
  // |DiskImageStatus| to poll for task completion.
  virtual void ResizeDiskImage(
      const vm_tools::concierge::ResizeDiskImageRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ResizeDiskImageResponse>
          callback) = 0;

  // Reclaims memory of the given VM.
  // |callback| is called after the method call finishes.
  virtual void ReclaimVmMemory(
      const vm_tools::concierge::ReclaimVmMemoryRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ReclaimVmMemoryResponse>
          callback) = 0;

  // Lists running VMs.
  // |callback| is called after the method call finishes.
  virtual void ListVms(
      const vm_tools::concierge::ListVmsRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ListVmsResponse>
          callback) = 0;

  virtual void GetVmLaunchAllowed(
      const vm_tools::concierge::GetVmLaunchAllowedRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::GetVmLaunchAllowedResponse> callback) = 0;

  // Swap out VMs.
  // |callback| is called after the method call finishes.
  virtual void SwapVm(
      const vm_tools::concierge::SwapVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::SwapVmResponse>
          callback) = 0;

  virtual void InstallPflash(
      base::ScopedFD fd,
      const vm_tools::concierge::InstallPflashRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::InstallPflashResponse>
          callback) = 0;

  // Enables or disables aggressive balloon.
  // |callback| is called after the method call finishes.
  virtual void AggressiveBalloon(
      const vm_tools::concierge::AggressiveBalloonRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::AggressiveBalloonResponse> callback) = 0;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  // FakeCiceroneClient must have already been initialized before calling
  // this function.
  static void InitializeFake();

  // Creates and initializes a fake global instance if not already created.
  // |fake_cicerone_client| must outlive the fake concierge client when it is
  // not null. When |fake_cicerone_client| is null, FakeConciergeClient won't
  // notify FakeCiceroneClient at all.
  static void InitializeFake(FakeCiceroneClient* fake_cicerone_client);

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ConciergeClient* Get();

  ~ConciergeClient() override;

 protected:
  // Initialize() should be used instead.
  ConciergeClient();
};

}  // namespace ash

namespace base {

template <>
struct ScopedObservationTraits<ash::ConciergeClient,
                               ash::ConciergeClient::VmObserver> {
  static void AddObserver(ash::ConciergeClient* source,
                          ash::ConciergeClient::VmObserver* observer) {
    source->AddVmObserver(observer);
  }
  static void RemoveObserver(ash::ConciergeClient* source,
                             ash::ConciergeClient::VmObserver* observer) {
    source->RemoveVmObserver(observer);
  }
};

template <>
struct ScopedObservationTraits<ash::ConciergeClient,
                               ash::ConciergeClient::DiskImageObserver> {
  static void AddObserver(ash::ConciergeClient* source,
                          ash::ConciergeClient::DiskImageObserver* observer) {
    source->AddDiskImageObserver(observer);
  }
  static void RemoveObserver(
      ash::ConciergeClient* source,
      ash::ConciergeClient::DiskImageObserver* observer) {
    source->RemoveDiskImageObserver(observer);
  }
};

}  // namespace base

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CONCIERGE_CONCIERGE_CLIENT_H_
