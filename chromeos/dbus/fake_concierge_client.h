// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_CONCIERGE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_CONCIERGE_CLIENT_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/dbus/cicerone_client.h"
#include "chromeos/dbus/concierge_client.h"

namespace chromeos {

// FakeConciergeClient is a light mock of ConciergeClient used for testing.
class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeConciergeClient
    : public ConciergeClient {
 public:
  FakeConciergeClient();
  ~FakeConciergeClient() override;

  // ConciergeClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void AddVmObserver(VmObserver* observer) override;
  void RemoveVmObserver(VmObserver* observer) override;
  void AddContainerObserver(ContainerObserver* observer) override;
  void RemoveContainerObserver(ContainerObserver* observer) override;
  void AddDiskImageObserver(DiskImageObserver* observer) override;
  void RemoveDiskImageObserver(DiskImageObserver* observer) override;

  bool IsVmStartedSignalConnected() override;
  bool IsVmStoppedSignalConnected() override;
  bool IsContainerStartupFailedSignalConnected() override;
  bool IsDiskImageProgressSignalConnected() override;
  void CreateDiskImage(
      const vm_tools::concierge::CreateDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse> callback)
      override;
  void CreateDiskImageWithFd(
      base::ScopedFD fd,
      const vm_tools::concierge::CreateDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse> callback)
      override;
  void DestroyDiskImage(
      const vm_tools::concierge::DestroyDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::DestroyDiskImageResponse>
          callback) override;
  // Fake version of the method that imports a VM disk image.
  // This function can fake a series of callbacks. It always first runs the
  // callback provided as an argument, and then optionally a series of fake
  // status signal callbacks (use set_disk_image_status_signals to set up).
  void ImportDiskImage(
      base::ScopedFD fd,
      const vm_tools::concierge::ImportDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::ImportDiskImageResponse> callback)
      override;
  void CancelDiskImageOperation(
      const vm_tools::concierge::CancelDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::CancelDiskImageResponse> callback)
      override;
  void DiskImageStatus(
      const vm_tools::concierge::DiskImageStatusRequest& request,
      DBusMethodCallback<vm_tools::concierge::DiskImageStatusResponse> callback)
      override;
  void ListVmDisks(const vm_tools::concierge::ListVmDisksRequest& request,
                   DBusMethodCallback<vm_tools::concierge::ListVmDisksResponse>
                       callback) override;
  void StartTerminaVm(const vm_tools::concierge::StartVmRequest& request,
                      DBusMethodCallback<vm_tools::concierge::StartVmResponse>
                          callback) override;
  void StopVm(const vm_tools::concierge::StopVmRequest& request,
              DBusMethodCallback<vm_tools::concierge::StopVmResponse> callback)
      override;
  void SuspendVm(const vm_tools::concierge::SuspendVmRequest& request,
                 DBusMethodCallback<vm_tools::concierge::SuspendVmResponse>
                     callback) override;
  void ResumeVm(const vm_tools::concierge::ResumeVmRequest& request,
                DBusMethodCallback<vm_tools::concierge::ResumeVmResponse>
                    callback) override;
  void GetVmInfo(const vm_tools::concierge::GetVmInfoRequest& request,
                 DBusMethodCallback<vm_tools::concierge::GetVmInfoResponse>
                     callback) override;
  void GetVmEnterpriseReportingInfo(
      const vm_tools::concierge::GetVmEnterpriseReportingInfoRequest& request,
      DBusMethodCallback<
          vm_tools::concierge::GetVmEnterpriseReportingInfoResponse> callback)
      override;
  void SetVmCpuRestriction(
      const vm_tools::concierge::SetVmCpuRestrictionRequest& request,
      DBusMethodCallback<vm_tools::concierge::SetVmCpuRestrictionResponse>
          callback) override;
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;
  void GetContainerSshKeys(
      const vm_tools::concierge::ContainerSshKeysRequest& request,
      DBusMethodCallback<vm_tools::concierge::ContainerSshKeysResponse>
          callback) override;
  void AttachUsbDevice(
      base::ScopedFD fd,
      const vm_tools::concierge::AttachUsbDeviceRequest& request,
      DBusMethodCallback<vm_tools::concierge::AttachUsbDeviceResponse> callback)
      override;
  void DetachUsbDevice(
      const vm_tools::concierge::DetachUsbDeviceRequest& request,
      DBusMethodCallback<vm_tools::concierge::DetachUsbDeviceResponse> callback)
      override;
  void StartArcVm(const vm_tools::concierge::StartArcVmRequest& request,
                  DBusMethodCallback<vm_tools::concierge::StartVmResponse>
                      callback) override;
  void ResizeDiskImage(
      const vm_tools::concierge::ResizeDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::ResizeDiskImageResponse> callback)
      override;

  void SetVmId(const vm_tools::concierge::SetVmIdRequest& request,
               DBusMethodCallback<vm_tools::concierge::SetVmIdResponse>
                   callback) override;

  const base::ObserverList<Observer>& observer_list() const {
    return observer_list_;
  }
  const base::ObserverList<VmObserver>::Unchecked& vm_observer_list() const {
    return vm_observer_list_;
  }
  const base::ObserverList<ContainerObserver>::Unchecked&
  container_observer_list() const {
    return container_observer_list_;
  }
  const base::ObserverList<DiskImageObserver>::Unchecked&
  disk_image_observer_list() const {
    return disk_image_observer_list_;
  }

  bool wait_for_service_to_be_available_called() const {
    return wait_for_service_to_be_available_called_;
  }
  bool create_disk_image_called() const { return create_disk_image_called_; }
  bool destroy_disk_image_called() const { return destroy_disk_image_called_; }
  bool import_disk_image_called() const { return import_disk_image_called_; }
  bool list_vm_disks_called() const { return list_vm_disks_called_; }
  bool start_termina_vm_called() const { return start_termina_vm_called_; }
  bool stop_vm_called() const { return stop_vm_called_; }
  bool get_vm_info_called() const { return get_vm_info_called_; }
  bool get_vm_enterprise_reporting_info_called() const {
    return get_vm_enterprise_reporting_info_called_;
  }
  bool get_container_ssh_keys_called() const {
    return get_container_ssh_keys_called_;
  }
  bool attach_usb_device_called() const { return attach_usb_device_called_; }
  bool detach_usb_device_called() const { return detach_usb_device_called_; }
  bool start_arc_vm_called() const { return start_arc_vm_called_; }
  bool resize_disk_image_called() const { return resize_disk_image_called_; }
  void set_vm_started_signal_connected(bool connected) {
    is_vm_started_signal_connected_ = connected;
  }
  void set_vm_stopped_signal_connected(bool connected) {
    is_vm_stopped_signal_connected_ = connected;
  }
  void set_container_startup_failed_signal_connected(bool connected) {
    is_container_startup_failed_signal_connected_ = connected;
  }
  void set_disk_image_progress_signal_connected(bool connected) {
    is_disk_image_progress_signal_connected_ = connected;
  }
  void set_wait_for_service_to_be_available_response(
      bool wait_for_service_to_be_available_response) {
    wait_for_service_to_be_available_response_ =
        wait_for_service_to_be_available_response;
  }
  void set_create_disk_image_response(
      base::Optional<vm_tools::concierge::CreateDiskImageResponse>
          create_disk_image_response) {
    create_disk_image_response_ = create_disk_image_response;
  }
  void set_destroy_disk_image_response(
      base::Optional<vm_tools::concierge::DestroyDiskImageResponse>
          destroy_disk_image_response) {
    destroy_disk_image_response_ = destroy_disk_image_response;
  }
  void set_import_disk_image_response(
      base::Optional<vm_tools::concierge::ImportDiskImageResponse>
          import_disk_image_response) {
    import_disk_image_response_ = import_disk_image_response;
  }
  void set_cancel_disk_image_response(
      base::Optional<vm_tools::concierge::CancelDiskImageResponse>
          cancel_disk_image_response) {
    cancel_disk_image_response_ = cancel_disk_image_response;
  }
  void set_disk_image_status_response(
      base::Optional<vm_tools::concierge::DiskImageStatusResponse>
          disk_image_status_response) {
    disk_image_status_response_ = disk_image_status_response;
  }
  void set_list_vm_disks_response(
      base::Optional<vm_tools::concierge::ListVmDisksResponse>
          list_vm_disks_response) {
    list_vm_disks_response_ = list_vm_disks_response;
  }
  void set_start_vm_response(
      base::Optional<vm_tools::concierge::StartVmResponse> start_vm_response) {
    start_vm_response_ = start_vm_response;
  }
  void set_stop_vm_response(
      base::Optional<vm_tools::concierge::StopVmResponse> stop_vm_response) {
    stop_vm_response_ = stop_vm_response;
  }
  void set_get_vm_info_response(
      base::Optional<vm_tools::concierge::GetVmInfoResponse>
          get_vm_info_response) {
    get_vm_info_response_ = get_vm_info_response;
  }
  void set_get_vm_enterprise_reporting_info_response(
      base::Optional<vm_tools::concierge::GetVmEnterpriseReportingInfoResponse>
          get_vm_enterprise_reporting_info_response) {
    get_vm_enterprise_reporting_info_response_ =
        get_vm_enterprise_reporting_info_response;
  }
  void set_set_vm_cpu_restriction_response(
      base::Optional<vm_tools::concierge::SetVmCpuRestrictionResponse>
          set_vm_cpu_restriction_response) {
    set_vm_cpu_restriction_response_ = set_vm_cpu_restriction_response;
  }
  void set_container_ssh_keys_response(
      base::Optional<vm_tools::concierge::ContainerSshKeysResponse>
          container_ssh_keys_response) {
    container_ssh_keys_response_ = container_ssh_keys_response;
  }
  void set_attach_usb_device_response(
      base::Optional<vm_tools::concierge::AttachUsbDeviceResponse>
          attach_usb_device_response) {
    attach_usb_device_response_ = attach_usb_device_response;
  }
  void set_detach_usb_device_response(
      base::Optional<vm_tools::concierge::DetachUsbDeviceResponse>
          detach_usb_device_response) {
    detach_usb_device_response_ = detach_usb_device_response;
  }
  void set_disk_image_status_signals(
      const std::vector<vm_tools::concierge::DiskImageStatusResponse>&
          disk_image_status_signals) {
    disk_image_status_signals_ = disk_image_status_signals;
  }
  void set_resize_disk_image_response(
      const vm_tools::concierge::ResizeDiskImageResponse&
          resize_disk_image_response) {
    resize_disk_image_response_ = resize_disk_image_response;
  }

  void NotifyVmStarted(const vm_tools::concierge::VmStartedSignal& signal);
  void NotifyVmStopped(const vm_tools::concierge::VmStoppedSignal& signal);
  bool HasVmObservers() const;

  void NotifyConciergeStopped();
  void NotifyConciergeStarted();

 protected:
  void Init(dbus::Bus* bus) override {}

 private:
  void InitializeProtoResponses();

  void NotifyTremplinStarted(
      const vm_tools::cicerone::TremplinStartedSignal& signal);

  // Notifies observers with a sequence of DiskImageStatus signals.
  void NotifyDiskImageProgress();
  // Notifies observers with a DiskImageStatus signal.
  void OnDiskImageProgress(
      const vm_tools::concierge::DiskImageStatusResponse& signal);

  bool wait_for_service_to_be_available_called_ = false;
  bool create_disk_image_called_ = false;
  bool destroy_disk_image_called_ = false;
  bool import_disk_image_called_ = false;
  bool disk_image_status_called_ = false;
  bool list_vm_disks_called_ = false;
  bool start_termina_vm_called_ = false;
  bool stop_vm_called_ = false;
  bool get_vm_info_called_ = false;
  bool get_vm_enterprise_reporting_info_called_ = false;
  bool set_vm_cpu_restriction_called_ = false;
  bool get_container_ssh_keys_called_ = false;
  bool attach_usb_device_called_ = false;
  bool detach_usb_device_called_ = false;
  bool start_arc_vm_called_ = false;
  bool resize_disk_image_called_ = false;
  bool set_vm_id_called_ = false;
  bool is_vm_started_signal_connected_ = true;
  bool is_vm_stopped_signal_connected_ = true;
  bool is_container_startup_failed_signal_connected_ = true;
  bool is_disk_image_progress_signal_connected_ = true;

  bool wait_for_service_to_be_available_response_ = true;
  base::Optional<vm_tools::concierge::CreateDiskImageResponse>
      create_disk_image_response_;
  base::Optional<vm_tools::concierge::DestroyDiskImageResponse>
      destroy_disk_image_response_;
  base::Optional<vm_tools::concierge::ImportDiskImageResponse>
      import_disk_image_response_;
  base::Optional<vm_tools::concierge::CancelDiskImageResponse>
      cancel_disk_image_response_;
  base::Optional<vm_tools::concierge::DiskImageStatusResponse>
      disk_image_status_response_;
  base::Optional<vm_tools::concierge::ListVmDisksResponse>
      list_vm_disks_response_;
  base::Optional<vm_tools::concierge::StartVmResponse> start_vm_response_;
  base::Optional<vm_tools::concierge::StopVmResponse> stop_vm_response_;
  base::Optional<vm_tools::concierge::SuspendVmResponse> suspend_vm_response_;
  base::Optional<vm_tools::concierge::ResumeVmResponse> resume_vm_response_;
  base::Optional<vm_tools::concierge::GetVmInfoResponse> get_vm_info_response_;
  base::Optional<vm_tools::concierge::GetVmEnterpriseReportingInfoResponse>
      get_vm_enterprise_reporting_info_response_;
  base::Optional<vm_tools::concierge::SetVmCpuRestrictionResponse>
      set_vm_cpu_restriction_response_;
  base::Optional<vm_tools::concierge::ContainerSshKeysResponse>
      container_ssh_keys_response_;
  base::Optional<vm_tools::concierge::AttachUsbDeviceResponse>
      attach_usb_device_response_;
  base::Optional<vm_tools::concierge::DetachUsbDeviceResponse>
      detach_usb_device_response_;
  base::Optional<vm_tools::concierge::ResizeDiskImageResponse>
      resize_disk_image_response_;
  base::Optional<vm_tools::concierge::SetVmIdResponse> set_vm_id_response_;

  // Can be set to fake a series of disk image status signals.
  std::vector<vm_tools::concierge::DiskImageStatusResponse>
      disk_image_status_signals_;

  base::ObserverList<Observer> observer_list_;

  base::ObserverList<VmObserver>::Unchecked vm_observer_list_;

  base::ObserverList<ContainerObserver>::Unchecked container_observer_list_;

  base::ObserverList<DiskImageObserver>::Unchecked disk_image_observer_list_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeConciergeClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeConciergeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_CONCIERGE_CLIENT_H_
