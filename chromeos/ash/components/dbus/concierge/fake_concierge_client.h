// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CONCIERGE_FAKE_CONCIERGE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CONCIERGE_FAKE_CONCIERGE_CLIENT_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"

namespace ash {

class FakeCiceroneClient;

// FakeConciergeClient is a light mock of ConciergeClient used for testing.
class COMPONENT_EXPORT(CONCIERGE) FakeConciergeClient : public ConciergeClient {
 public:
  // Returns the fake global instance if initialized. May return null.
  static FakeConciergeClient* Get();

  FakeConciergeClient(const FakeConciergeClient&) = delete;
  FakeConciergeClient& operator=(const FakeConciergeClient&) = delete;

  // ConciergeClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void AddVmObserver(VmObserver* observer) override;
  void RemoveVmObserver(VmObserver* observer) override;
  void AddDiskImageObserver(DiskImageObserver* observer) override;
  void RemoveDiskImageObserver(DiskImageObserver* observer) override;

  bool IsVmStartedSignalConnected() override;
  bool IsVmStoppedSignalConnected() override;
  bool IsVmStoppingSignalConnected() override;
  bool IsDiskImageProgressSignalConnected() override;
  void CreateDiskImage(
      const vm_tools::concierge::CreateDiskImageRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse>
          callback) override;
  void CreateDiskImageWithFd(
      base::ScopedFD fd,
      const vm_tools::concierge::CreateDiskImageRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse>
          callback) override;
  void DestroyDiskImage(
      const vm_tools::concierge::DestroyDiskImageRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::DestroyDiskImageResponse> callback) override;
  // Fake version of the method that imports a VM disk image.
  // This function can fake a series of callbacks. It always first runs the
  // callback provided as an argument, and then optionally a series of fake
  // status signal callbacks (use set_disk_image_status_signals to set up).
  void ImportDiskImage(
      base::ScopedFD fd,
      const vm_tools::concierge::ImportDiskImageRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ImportDiskImageResponse>
          callback) override;
  void ExportDiskImage(
      std::vector<base::ScopedFD> fds,
      const vm_tools::concierge::ExportDiskImageRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ExportDiskImageResponse>
          callback) override;
  void CancelDiskImageOperation(
      const vm_tools::concierge::CancelDiskImageRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::CancelDiskImageResponse>
          callback) override;
  void DiskImageStatus(
      const vm_tools::concierge::DiskImageStatusRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::DiskImageStatusResponse>
          callback) override;
  void ListVmDisks(
      const vm_tools::concierge::ListVmDisksRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ListVmDisksResponse>
          callback) override;
  void StartVm(
      const vm_tools::concierge::StartVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
          callback) override;
  void StartVmWithFd(
      base::ScopedFD fd,
      const vm_tools::concierge::StartVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
          callback) override;
  void StopVm(const vm_tools::concierge::StopVmRequest& request,
              chromeos::DBusMethodCallback<vm_tools::concierge::StopVmResponse>
                  callback) override;
  void SuspendVm(
      const vm_tools::concierge::SuspendVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::SuspendVmResponse>
          callback) override;
  void ResumeVm(
      const vm_tools::concierge::ResumeVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ResumeVmResponse>
          callback) override;
  void GetVmInfo(
      const vm_tools::concierge::GetVmInfoRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::GetVmInfoResponse>
          callback) override;
  void GetVmEnterpriseReportingInfo(
      const vm_tools::concierge::GetVmEnterpriseReportingInfoRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::GetVmEnterpriseReportingInfoResponse> callback)
      override;
  void ArcVmCompleteBoot(
      const vm_tools::concierge::ArcVmCompleteBootRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::ArcVmCompleteBootResponse> callback) override;
  void SetVmCpuRestriction(
      const vm_tools::concierge::SetVmCpuRestrictionRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::SetVmCpuRestrictionResponse> callback) override;
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;
  void AttachUsbDevice(
      base::ScopedFD fd,
      const vm_tools::concierge::AttachUsbDeviceRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::AttachUsbDeviceResponse>
          callback) override;
  void DetachUsbDevice(
      const vm_tools::concierge::DetachUsbDeviceRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::DetachUsbDeviceResponse>
          callback) override;
  void StartArcVm(
      const vm_tools::concierge::StartArcVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
          callback) override;
  void ResizeDiskImage(
      const vm_tools::concierge::ResizeDiskImageRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ResizeDiskImageResponse>
          callback) override;

  void ReclaimVmMemory(
      const vm_tools::concierge::ReclaimVmMemoryRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ReclaimVmMemoryResponse>
          callback) override;

  void ListVms(
      const vm_tools::concierge::ListVmsRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ListVmsResponse>
          callback) override;

  void GetVmLaunchAllowed(
      const vm_tools::concierge::GetVmLaunchAllowedRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::GetVmLaunchAllowedResponse> callback) override;

  void SwapVm(const vm_tools::concierge::SwapVmRequest& request,
              chromeos::DBusMethodCallback<vm_tools::concierge::SwapVmResponse>
                  callback) override;

  void InstallPflash(
      base::ScopedFD fd,
      const vm_tools::concierge::InstallPflashRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::InstallPflashResponse>
          callback) override;

  void AggressiveBalloon(
      const vm_tools::concierge::AggressiveBalloonRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::AggressiveBalloonResponse> callback) override;

  const base::ObserverList<Observer>& observer_list() const {
    return observer_list_;
  }
  const base::ObserverList<VmObserver>::UncheckedAndDanglingUntriaged&
  vm_observer_list() const {
    return vm_observer_list_;
  }
  const base::ObserverList<DiskImageObserver>::UncheckedAndDanglingUntriaged&
  disk_image_observer_list() const {
    return disk_image_observer_list_;
  }

  int wait_for_service_to_be_available_call_count() const {
    return wait_for_service_to_be_available_call_count_;
  }
  int create_disk_image_call_count() const {
    return create_disk_image_call_count_;
  }
  int destroy_disk_image_call_count() const {
    return destroy_disk_image_call_count_;
  }
  int import_disk_image_call_count() const {
    return import_disk_image_call_count_;
  }
  int export_disk_image_call_count() const {
    return export_disk_image_call_count_;
  }
  int list_vm_disks_call_count() const { return list_vm_disks_call_count_; }
  int start_vm_call_count() const { return start_vm_call_count_; }
  int stop_vm_call_count() const { return stop_vm_call_count_; }
  int get_vm_info_call_count() const { return get_vm_info_call_count_; }
  int get_vm_enterprise_reporting_info_call_count() const {
    return get_vm_enterprise_reporting_info_call_count_;
  }
  int arcvm_complete_boot_call_count() const {
    return arcvm_complete_boot_call_count_;
  }
  int get_container_ssh_keys_call_count() const {
    return get_container_ssh_keys_call_count_;
  }
  int attach_usb_device_call_count() const {
    return attach_usb_device_call_count_;
  }
  int detach_usb_device_call_count() const {
    return detach_usb_device_call_count_;
  }
  int start_arc_vm_call_count() const { return start_arc_vm_call_count_; }
  int resize_disk_image_call_count() const {
    return resize_disk_image_call_count_;
  }
  int reclaim_vm_memory_call_count() const {
    return reclaim_vm_memory_call_count_;
  }
  int list_vms_call_count() const { return list_vms_call_count_; }

  void set_vm_started_signal_connected(bool connected) {
    is_vm_started_signal_connected_ = connected;
  }
  void set_vm_stopped_signal_connected(bool connected) {
    is_vm_stopped_signal_connected_ = connected;
  }
  void set_vm_stopping_signal_connected(bool connected) {
    is_vm_stopping_signal_connected_ = connected;
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
      std::optional<vm_tools::concierge::CreateDiskImageResponse>
          create_disk_image_response) {
    create_disk_image_response_ = create_disk_image_response;
  }
  void set_destroy_disk_image_response(
      std::optional<vm_tools::concierge::DestroyDiskImageResponse>
          destroy_disk_image_response) {
    destroy_disk_image_response_ = destroy_disk_image_response;
  }
  void set_import_disk_image_response(
      std::optional<vm_tools::concierge::ImportDiskImageResponse>
          import_disk_image_response) {
    import_disk_image_response_ = import_disk_image_response;
  }
  void set_export_disk_image_response(
      std::optional<vm_tools::concierge::ExportDiskImageResponse>
          export_disk_image_response) {
    export_disk_image_response_ = export_disk_image_response;
  }
  void set_cancel_disk_image_response(
      std::optional<vm_tools::concierge::CancelDiskImageResponse>
          cancel_disk_image_response) {
    cancel_disk_image_response_ = cancel_disk_image_response;
  }
  void set_disk_image_status_response(
      std::optional<vm_tools::concierge::DiskImageStatusResponse>
          disk_image_status_response) {
    disk_image_status_response_ = disk_image_status_response;
  }
  void set_list_vm_disks_response(
      std::optional<vm_tools::concierge::ListVmDisksResponse>
          list_vm_disks_response) {
    list_vm_disks_response_ = list_vm_disks_response;
  }
  void set_start_vm_response(
      std::optional<vm_tools::concierge::StartVmResponse> start_vm_response) {
    start_vm_response_ = start_vm_response;
  }
  void set_stop_vm_response(
      std::optional<vm_tools::concierge::StopVmResponse> stop_vm_response) {
    stop_vm_response_ = stop_vm_response;
  }
  void set_get_vm_info_response(
      std::optional<vm_tools::concierge::GetVmInfoResponse>
          get_vm_info_response) {
    get_vm_info_response_ = get_vm_info_response;
  }
  void set_get_vm_enterprise_reporting_info_response(
      std::optional<vm_tools::concierge::GetVmEnterpriseReportingInfoResponse>
          get_vm_enterprise_reporting_info_response) {
    get_vm_enterprise_reporting_info_response_ =
        get_vm_enterprise_reporting_info_response;
  }
  void set_arcvm_complete_boot_response(
      std::optional<vm_tools::concierge::ArcVmCompleteBootResponse>
          arcvm_complete_boot_response) {
    arcvm_complete_boot_response_ = arcvm_complete_boot_response;
  }
  void set_set_vm_cpu_restriction_response(
      std::optional<vm_tools::concierge::SetVmCpuRestrictionResponse>
          set_vm_cpu_restriction_response) {
    set_vm_cpu_restriction_response_ = set_vm_cpu_restriction_response;
  }
  void set_attach_usb_device_response(
      std::optional<vm_tools::concierge::AttachUsbDeviceResponse>
          attach_usb_device_response) {
    attach_usb_device_response_ = attach_usb_device_response;
  }
  void set_detach_usb_device_response(
      std::optional<vm_tools::concierge::DetachUsbDeviceResponse>
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
  void set_reclaim_vm_memory_response(
      std::optional<vm_tools::concierge::ReclaimVmMemoryResponse>
          reclaim_vm_memory_response) {
    reclaim_vm_memory_response_ = reclaim_vm_memory_response;
  }
  void set_list_vms_response(
      std::optional<vm_tools::concierge::ListVmsResponse> list_vms_response) {
    list_vms_response_ = list_vms_response;
  }
  void set_get_vm_launch_allowed_response(
      std::optional<vm_tools::concierge::GetVmLaunchAllowedResponse>
          get_vm_launch_allowed_response) {
    get_vm_launch_allowed_response_ = get_vm_launch_allowed_response;
  }
  void set_swap_vm_response(
      std::optional<vm_tools::concierge::SwapVmResponse> swap_vm_response) {
    swap_vm_response_ = swap_vm_response;
  }
  void set_install_pflash_response(
      std::optional<vm_tools::concierge::InstallPflashResponse>
          install_pflash_response) {
    install_pflash_response_ = install_pflash_response;
  }
  void set_aggressive_balloon_response(
      std::optional<vm_tools::concierge::AggressiveBalloonResponse>
          aggressive_balloon_response) {
    aggressive_balloon_response_ = aggressive_balloon_response;
  }

  void set_send_create_disk_image_response_delay(base::TimeDelta delay) {
    send_create_disk_image_response_delay_ = delay;
  }
  void set_send_start_vm_response_delay(base::TimeDelta delay) {
    send_start_vm_response_delay_ = delay;
  }
  void set_send_tremplin_started_signal_delay(base::TimeDelta delay) {
    send_tremplin_started_signal_delay_ = delay;
  }
  void reset_get_vm_info_call_count() { get_vm_info_call_count_ = 0; }

  void NotifyDiskImageProgress(
      vm_tools::concierge::DiskImageStatusResponse signal);

  void NotifyVmStarted(const vm_tools::concierge::VmStartedSignal& signal);
  void NotifyVmStopped(const vm_tools::concierge::VmStoppedSignal& signal);
  void NotifyVmStopping(const vm_tools::concierge::VmStoppingSignal& signal);
  bool HasVmObservers() const;

  void NotifyConciergeStopped();
  void NotifyConciergeStarted();

 protected:
  friend class ConciergeClient;

  explicit FakeConciergeClient(FakeCiceroneClient* fake_cicerone_client);
  ~FakeConciergeClient() override;

  void Init(dbus::Bus* bus) override {}

 private:
  void InitializeProtoResponses();

  void NotifyTremplinStarted(
      const vm_tools::cicerone::TremplinStartedSignal& signal);

  // Notifies observers with a sequence of DiskImageStatus signals.
  void NotifyAllDiskImageProgress();
  // Notifies observers with a DiskImageStatus signal.
  void OnDiskImageProgress(
      const vm_tools::concierge::DiskImageStatusResponse& signal);

  const raw_ptr<FakeCiceroneClient, DanglingUntriaged> fake_cicerone_client_;

  int wait_for_service_to_be_available_call_count_ = 0;
  int create_disk_image_call_count_ = 0;
  int destroy_disk_image_call_count_ = 0;
  int import_disk_image_call_count_ = 0;
  int export_disk_image_call_count_ = 0;
  int disk_image_status_call_count_ = 0;
  int list_vm_disks_call_count_ = 0;
  int start_vm_call_count_ = 0;
  int stop_vm_call_count_ = 0;
  int get_vm_info_call_count_ = 0;
  int get_vm_enterprise_reporting_info_call_count_ = 0;
  int arcvm_complete_boot_call_count_ = 0;
  int set_vm_cpu_restriction_call_count_ = 0;
  int get_container_ssh_keys_call_count_ = 0;
  int attach_usb_device_call_count_ = 0;
  int detach_usb_device_call_count_ = 0;
  int start_arc_vm_call_count_ = 0;
  int resize_disk_image_call_count_ = 0;
  int set_vm_id_call_count_ = 0;
  int reclaim_vm_memory_call_count_ = 0;
  int list_vms_call_count_ = 0;

  bool is_vm_started_signal_connected_ = true;
  bool is_vm_stopped_signal_connected_ = true;
  bool is_vm_stopping_signal_connected_ = true;
  bool is_disk_image_progress_signal_connected_ = true;

  bool wait_for_service_to_be_available_response_ = true;
  std::optional<vm_tools::concierge::CreateDiskImageResponse>
      create_disk_image_response_;
  std::optional<vm_tools::concierge::DestroyDiskImageResponse>
      destroy_disk_image_response_;
  std::optional<vm_tools::concierge::ImportDiskImageResponse>
      import_disk_image_response_;
  std::optional<vm_tools::concierge::ExportDiskImageResponse>
      export_disk_image_response_;
  std::optional<vm_tools::concierge::CancelDiskImageResponse>
      cancel_disk_image_response_;
  std::optional<vm_tools::concierge::DiskImageStatusResponse>
      disk_image_status_response_;
  std::optional<vm_tools::concierge::ListVmDisksResponse>
      list_vm_disks_response_;
  std::optional<vm_tools::concierge::StartVmResponse> start_vm_response_;
  std::optional<vm_tools::concierge::StopVmResponse> stop_vm_response_;
  std::optional<vm_tools::concierge::SuspendVmResponse> suspend_vm_response_;
  std::optional<vm_tools::concierge::ResumeVmResponse> resume_vm_response_;
  std::optional<vm_tools::concierge::GetVmInfoResponse> get_vm_info_response_;
  std::optional<vm_tools::concierge::GetVmEnterpriseReportingInfoResponse>
      get_vm_enterprise_reporting_info_response_;
  std::optional<vm_tools::concierge::ArcVmCompleteBootResponse>
      arcvm_complete_boot_response_;
  std::optional<vm_tools::concierge::SetVmCpuRestrictionResponse>
      set_vm_cpu_restriction_response_;
  std::optional<vm_tools::concierge::AttachUsbDeviceResponse>
      attach_usb_device_response_;
  std::optional<vm_tools::concierge::DetachUsbDeviceResponse>
      detach_usb_device_response_;
  std::optional<vm_tools::concierge::ResizeDiskImageResponse>
      resize_disk_image_response_;
  std::optional<vm_tools::concierge::ReclaimVmMemoryResponse>
      reclaim_vm_memory_response_;
  std::optional<vm_tools::concierge::ListVmsResponse> list_vms_response_;
  std::optional<vm_tools::concierge::GetVmLaunchAllowedResponse>
      get_vm_launch_allowed_response_;
  std::optional<vm_tools::concierge::SwapVmResponse> swap_vm_response_;
  std::optional<vm_tools::concierge::InstallPflashResponse>
      install_pflash_response_;
  std::optional<vm_tools::concierge::AggressiveBalloonResponse>
      aggressive_balloon_response_;

  base::TimeDelta send_create_disk_image_response_delay_;
  base::TimeDelta send_start_vm_response_delay_;
  base::TimeDelta send_tremplin_started_signal_delay_;

  // Can be set to fake a series of disk image status signals.
  std::vector<vm_tools::concierge::DiskImageStatusResponse>
      disk_image_status_signals_;

  base::ObserverList<Observer> observer_list_{
      ConciergeClient::kObserverListPolicy};

  base::ObserverList<VmObserver>::UncheckedAndDanglingUntriaged
      vm_observer_list_{ConciergeClient::kObserverListPolicy};

  base::ObserverList<DiskImageObserver>::UncheckedAndDanglingUntriaged
      disk_image_observer_list_{ConciergeClient::kObserverListPolicy};

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeConciergeClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CONCIERGE_FAKE_CONCIERGE_CLIENT_H_
