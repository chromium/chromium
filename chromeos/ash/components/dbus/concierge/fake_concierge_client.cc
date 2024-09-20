// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"

namespace ash {

namespace {

FakeConciergeClient* g_instance = nullptr;

}  // namespace

// static
FakeConciergeClient* FakeConciergeClient::Get() {
  return g_instance;
}

FakeConciergeClient::FakeConciergeClient(
    FakeCiceroneClient* fake_cicerone_client)
    : fake_cicerone_client_(fake_cicerone_client) {
  DCHECK(!g_instance);
  g_instance = this;

  InitializeProtoResponses();
}

FakeConciergeClient::~FakeConciergeClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void FakeConciergeClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeConciergeClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeConciergeClient::NotifyConciergeStopped() {
  for (auto& observer : observer_list_) {
    observer.ConciergeServiceStopped();
  }
}
void FakeConciergeClient::NotifyConciergeStarted() {
  for (auto& observer : observer_list_) {
    observer.ConciergeServiceStarted();
  }
}

void FakeConciergeClient::AddVmObserver(VmObserver* observer) {
  vm_observer_list_.AddObserver(observer);
}

void FakeConciergeClient::RemoveVmObserver(VmObserver* observer) {
  vm_observer_list_.RemoveObserver(observer);
}

void FakeConciergeClient::AddDiskImageObserver(DiskImageObserver* observer) {
  disk_image_observer_list_.AddObserver(observer);
}

void FakeConciergeClient::RemoveDiskImageObserver(DiskImageObserver* observer) {
  disk_image_observer_list_.RemoveObserver(observer);
}

bool FakeConciergeClient::IsVmStartedSignalConnected() {
  return is_vm_started_signal_connected_;
}

bool FakeConciergeClient::IsVmStoppedSignalConnected() {
  return is_vm_stopped_signal_connected_;
}

bool FakeConciergeClient::IsVmStoppingSignalConnected() {
  return is_vm_stopping_signal_connected_;
}

bool FakeConciergeClient::IsDiskImageProgressSignalConnected() {
  return is_disk_image_progress_signal_connected_;
}

void FakeConciergeClient::CreateDiskImage(
    const vm_tools::concierge::CreateDiskImageRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse>
        callback) {
  create_disk_image_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), create_disk_image_response_),
      send_create_disk_image_response_delay_);
}

void FakeConciergeClient::CreateDiskImageWithFd(
    base::ScopedFD fd,
    const vm_tools::concierge::CreateDiskImageRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse>
        callback) {
  create_disk_image_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), create_disk_image_response_));
}

void FakeConciergeClient::DestroyDiskImage(
    const vm_tools::concierge::DestroyDiskImageRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::DestroyDiskImageResponse>
        callback) {
  destroy_disk_image_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), destroy_disk_image_response_));
}

void FakeConciergeClient::ImportDiskImage(
    base::ScopedFD fd,
    const vm_tools::concierge::ImportDiskImageRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::ImportDiskImageResponse>
        callback) {
  import_disk_image_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), import_disk_image_response_));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeConciergeClient::NotifyAllDiskImageProgress,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FakeConciergeClient::ExportDiskImage(
    std::vector<base::ScopedFD> fds,
    const vm_tools::concierge::ExportDiskImageRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::ExportDiskImageResponse>
        callback) {
  export_disk_image_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), export_disk_image_response_));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeConciergeClient::NotifyAllDiskImageProgress,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FakeConciergeClient::CancelDiskImageOperation(
    const vm_tools::concierge::CancelDiskImageRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::CancelDiskImageResponse>
        callback) {
  // Removes signals sent during disk image import.
  disk_image_status_signals_.clear();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), cancel_disk_image_response_));
}
void FakeConciergeClient::NotifyDiskImageProgress(
    vm_tools::concierge::DiskImageStatusResponse signal) {
  OnDiskImageProgress(signal);
}

void FakeConciergeClient::NotifyAllDiskImageProgress() {
  // Trigger DiskImageStatus signals.
  for (auto const& signal : disk_image_status_signals_) {
    OnDiskImageProgress(signal);
  }
}

void FakeConciergeClient::OnDiskImageProgress(
    const vm_tools::concierge::DiskImageStatusResponse& signal) {
  for (auto& observer : disk_image_observer_list_) {
    observer.OnDiskImageProgress(signal);
  }
}

void FakeConciergeClient::DiskImageStatus(
    const vm_tools::concierge::DiskImageStatusRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::DiskImageStatusResponse>
        callback) {
  disk_image_status_call_count_++;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), disk_image_status_response_));
}

void FakeConciergeClient::ListVmDisks(
    const vm_tools::concierge::ListVmDisksRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::ListVmDisksResponse>
        callback) {
  list_vm_disks_call_count_++;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), list_vm_disks_response_));
}

void FakeConciergeClient::StartVm(
    const vm_tools::concierge::StartVmRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
        callback) {
  start_vm_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), start_vm_response_),
      send_start_vm_response_delay_);

  if (!start_vm_response_ ||
      start_vm_response_->status() != vm_tools::concierge::VM_STATUS_STARTING) {
    // Don't send the tremplin signal unless the VM was STARTING.
    return;
  }

  // Trigger CiceroneClient::Observer::NotifyTremplinStartedSignal.
  vm_tools::cicerone::TremplinStartedSignal tremplin_started_signal;
  tremplin_started_signal.set_vm_name(request.name());
  tremplin_started_signal.set_owner_id(request.owner_id());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeConciergeClient::NotifyTremplinStarted,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(tremplin_started_signal)),
      send_tremplin_started_signal_delay_);

  // Trigger VmStartedSignal
  vm_tools::concierge::VmStartedSignal vm_started_signal;
  vm_started_signal.set_name(request.name());
  vm_started_signal.set_owner_id(request.owner_id());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeConciergeClient::NotifyVmStarted,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(vm_started_signal)));
}

void FakeConciergeClient::StartVmWithFd(
    base::ScopedFD fd,
    const vm_tools::concierge::StartVmRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
        callback) {
  StartVm(std::move(request), std::move(callback));
}

void FakeConciergeClient::NotifyTremplinStarted(
    const vm_tools::cicerone::TremplinStartedSignal& signal) {
  DCHECK(fake_cicerone_client_)
      << "|fake_cicerone_client_| is not set. Your test has to call "
      << "ConciergeClient::InitializeFake() with a non-null pointer to "
      << "CiceroniClient e.g. the one returned from FakeCiceroneClient::Get().";
  if (fake_cicerone_client_)
    fake_cicerone_client_->NotifyTremplinStarted(signal);
}

void FakeConciergeClient::StopVm(
    const vm_tools::concierge::StopVmRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::StopVmResponse>
        callback) {
  stop_vm_call_count_++;
  vm_tools::concierge::VmStoppedSignal signal;
  signal.set_name(request.name());
  signal.set_owner_id(request.owner_id());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeConciergeClient::NotifyVmStopped,
                     weak_ptr_factory_.GetWeakPtr(), std::move(signal)));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), stop_vm_response_));
}

void FakeConciergeClient::SuspendVm(
    const vm_tools::concierge::SuspendVmRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::SuspendVmResponse>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), suspend_vm_response_));
}

void FakeConciergeClient::ResumeVm(
    const vm_tools::concierge::ResumeVmRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::ResumeVmResponse>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), resume_vm_response_));
}

void FakeConciergeClient::GetVmInfo(
    const vm_tools::concierge::GetVmInfoRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::GetVmInfoResponse>
        callback) {
  get_vm_info_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), get_vm_info_response_));
}

void FakeConciergeClient::GetVmEnterpriseReportingInfo(
    const vm_tools::concierge::GetVmEnterpriseReportingInfoRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::concierge::GetVmEnterpriseReportingInfoResponse> callback) {
  get_vm_enterprise_reporting_info_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                get_vm_enterprise_reporting_info_response_));
}

void FakeConciergeClient::ArcVmCompleteBoot(
    const vm_tools::concierge::ArcVmCompleteBootRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::ArcVmCompleteBootResponse>
        callback) {
  arcvm_complete_boot_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), arcvm_complete_boot_response_));
}

void FakeConciergeClient::SetVmCpuRestriction(
    const vm_tools::concierge::SetVmCpuRestrictionRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::concierge::SetVmCpuRestrictionResponse> callback) {
  set_vm_cpu_restriction_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), set_vm_cpu_restriction_response_));
}

void FakeConciergeClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  wait_for_service_to_be_available_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                wait_for_service_to_be_available_response_));
}

void FakeConciergeClient::AttachUsbDevice(
    base::ScopedFD fd,
    const vm_tools::concierge::AttachUsbDeviceRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::AttachUsbDeviceResponse>
        callback) {
  attach_usb_device_call_count_++;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), attach_usb_device_response_));
}

void FakeConciergeClient::DetachUsbDevice(
    const vm_tools::concierge::DetachUsbDeviceRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::DetachUsbDeviceResponse>
        callback) {
  detach_usb_device_call_count_++;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), detach_usb_device_response_));
}

void FakeConciergeClient::StartArcVm(
    const vm_tools::concierge::StartArcVmRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
        callback) {
  start_arc_vm_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), start_vm_response_));
}

void FakeConciergeClient::ResizeDiskImage(
    const vm_tools::concierge::ResizeDiskImageRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::ResizeDiskImageResponse>
        callback) {
  resize_disk_image_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), resize_disk_image_response_));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeConciergeClient::NotifyAllDiskImageProgress,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FakeConciergeClient::ReclaimVmMemory(
    const vm_tools::concierge::ReclaimVmMemoryRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::ReclaimVmMemoryResponse>
        callback) {
  reclaim_vm_memory_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), reclaim_vm_memory_response_));
}

void FakeConciergeClient::ListVms(
    const vm_tools::concierge::ListVmsRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::ListVmsResponse>
        callback) {
  list_vms_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), list_vms_response_));
}

void FakeConciergeClient::GetVmLaunchAllowed(
    const vm_tools::concierge::GetVmLaunchAllowedRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::concierge::GetVmLaunchAllowedResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), get_vm_launch_allowed_response_));
}

void FakeConciergeClient::SwapVm(
    const vm_tools::concierge::SwapVmRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::SwapVmResponse>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), swap_vm_response_));
}

void FakeConciergeClient::InstallPflash(
    base::ScopedFD fd,
    const vm_tools::concierge::InstallPflashRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::InstallPflashResponse>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), install_pflash_response_));
}

void FakeConciergeClient::AggressiveBalloon(
    const vm_tools::concierge::AggressiveBalloonRequest& request,
    chromeos::DBusMethodCallback<vm_tools::concierge::AggressiveBalloonResponse>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), aggressive_balloon_response_));
}

void FakeConciergeClient::NotifyVmStarted(
    const vm_tools::concierge::VmStartedSignal& signal) {
  // Now GetVmInfo can return success.
  get_vm_info_response_->set_success(true);
  for (auto& observer : vm_observer_list_)
    observer.OnVmStarted(signal);
}

void FakeConciergeClient::NotifyVmStopped(
    const vm_tools::concierge::VmStoppedSignal& signal) {
  // Now GetVmInfo can no longer succeed.
  get_vm_info_response_->set_success(false);
  for (auto& observer : vm_observer_list_)
    observer.OnVmStopped(signal);
}

void FakeConciergeClient::NotifyVmStopping(
    const vm_tools::concierge::VmStoppingSignal& signal) {
  for (auto& observer : vm_observer_list_) {
    observer.OnVmStopping(signal);
  }
}

bool FakeConciergeClient::HasVmObservers() const {
  return !vm_observer_list_.empty();
}

void FakeConciergeClient::InitializeProtoResponses() {
  create_disk_image_response_.emplace();
  create_disk_image_response_->set_status(
      vm_tools::concierge::DISK_STATUS_CREATED);
  create_disk_image_response_->set_disk_path("foo");

  destroy_disk_image_response_.emplace();
  destroy_disk_image_response_->set_status(
      vm_tools::concierge::DISK_STATUS_DESTROYED);

  import_disk_image_response_.emplace();
  export_disk_image_response_.emplace();
  export_disk_image_response_->set_status(
      vm_tools::concierge::DISK_STATUS_IN_PROGRESS);
  cancel_disk_image_response_.emplace();
  cancel_disk_image_response_->set_success(true);
  disk_image_status_response_.emplace();

  list_vm_disks_response_.emplace();
  list_vm_disks_response_->set_success(true);

  start_vm_response_.emplace();
  start_vm_response_->set_status(vm_tools::concierge::VM_STATUS_STARTING);
  start_vm_response_->set_mount_result(
      vm_tools::concierge::StartVmResponse::SUCCESS);

  stop_vm_response_.emplace();
  stop_vm_response_->set_success(true);

  suspend_vm_response_.emplace();
  suspend_vm_response_->set_success(true);

  resume_vm_response_.emplace();
  resume_vm_response_->set_success(true);

  get_vm_info_response_.emplace();
  get_vm_info_response_->set_success(false);
  get_vm_info_response_->mutable_vm_info()->set_seneschal_server_handle(1);

  get_vm_enterprise_reporting_info_response_.emplace();

  arcvm_complete_boot_response_.emplace();
  arcvm_complete_boot_response_->set_result(
      vm_tools::concierge::ArcVmCompleteBootResult::SUCCESS);

  set_vm_cpu_restriction_response_.emplace();

  attach_usb_device_response_.emplace();
  attach_usb_device_response_->set_success(true);
  attach_usb_device_response_->set_guest_port(0);

  detach_usb_device_response_.emplace();
  detach_usb_device_response_->set_success(true);

  install_pflash_response_.emplace();
  install_pflash_response_->set_success(true);
}

}  // namespace ash
