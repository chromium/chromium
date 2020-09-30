// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_concierge_client.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"

namespace chromeos {

FakeConciergeClient::FakeConciergeClient() {
  InitializeProtoResponses();
}
FakeConciergeClient::~FakeConciergeClient() = default;

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

void FakeConciergeClient::AddContainerObserver(ContainerObserver* observer) {
  container_observer_list_.AddObserver(observer);
}

void FakeConciergeClient::RemoveContainerObserver(ContainerObserver* observer) {
  container_observer_list_.RemoveObserver(observer);
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

bool FakeConciergeClient::IsContainerStartupFailedSignalConnected() {
  return is_container_startup_failed_signal_connected_;
}

bool FakeConciergeClient::IsDiskImageProgressSignalConnected() {
  return is_disk_image_progress_signal_connected_;
}

void FakeConciergeClient::CreateDiskImage(
    const vm_tools::concierge::CreateDiskImageRequest& request,
    DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse> callback) {
  create_disk_image_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), create_disk_image_response_));
}

void FakeConciergeClient::CreateDiskImageWithFd(
    base::ScopedFD fd,
    const vm_tools::concierge::CreateDiskImageRequest& request,
    DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse> callback) {
  create_disk_image_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), create_disk_image_response_));
}

void FakeConciergeClient::DestroyDiskImage(
    const vm_tools::concierge::DestroyDiskImageRequest& request,
    DBusMethodCallback<vm_tools::concierge::DestroyDiskImageResponse>
        callback) {
  destroy_disk_image_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), destroy_disk_image_response_));
}

void FakeConciergeClient::ImportDiskImage(
    base::ScopedFD fd,
    const vm_tools::concierge::ImportDiskImageRequest& request,
    DBusMethodCallback<vm_tools::concierge::ImportDiskImageResponse> callback) {
  import_disk_image_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), import_disk_image_response_));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeConciergeClient::NotifyDiskImageProgress,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FakeConciergeClient::CancelDiskImageOperation(
    const vm_tools::concierge::CancelDiskImageRequest& request,
    DBusMethodCallback<vm_tools::concierge::CancelDiskImageResponse> callback) {
  // Removes signals sent during disk image import.
  disk_image_status_signals_.clear();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), cancel_disk_image_response_));
}

void FakeConciergeClient::NotifyDiskImageProgress() {
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
    DBusMethodCallback<vm_tools::concierge::DiskImageStatusResponse> callback) {
  disk_image_status_called_ = true;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), disk_image_status_response_));
}

void FakeConciergeClient::ListVmDisks(
    const vm_tools::concierge::ListVmDisksRequest& request,
    DBusMethodCallback<vm_tools::concierge::ListVmDisksResponse> callback) {
  list_vm_disks_called_ = true;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), list_vm_disks_response_));
}

void FakeConciergeClient::StartTerminaVm(
    const vm_tools::concierge::StartVmRequest& request,
    DBusMethodCallback<vm_tools::concierge::StartVmResponse> callback) {
  start_termina_vm_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), start_vm_response_));

  if (!start_vm_response_ ||
      start_vm_response_->status() != vm_tools::concierge::VM_STATUS_STARTING) {
    // Don't send the tremplin signal unless the VM was STARTING.
    return;
  }

  // Trigger CiceroneClient::Observer::NotifyTremplinStartedSignal.
  vm_tools::cicerone::TremplinStartedSignal tremplin_started_signal;
  tremplin_started_signal.set_vm_name(request.name());
  tremplin_started_signal.set_owner_id(request.owner_id());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeConciergeClient::NotifyTremplinStarted,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(tremplin_started_signal)));
}

void FakeConciergeClient::NotifyTremplinStarted(
    const vm_tools::cicerone::TremplinStartedSignal& signal) {
  static_cast<FakeCiceroneClient*>(
      chromeos::DBusThreadManager::Get()->GetCiceroneClient())
      ->NotifyTremplinStarted(signal);
}

void FakeConciergeClient::StopVm(
    const vm_tools::concierge::StopVmRequest& request,
    DBusMethodCallback<vm_tools::concierge::StopVmResponse> callback) {
  stop_vm_called_ = true;
  vm_tools::concierge::VmStoppedSignal signal;
  signal.set_name(request.name());
  signal.set_owner_id(request.owner_id());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeConciergeClient::NotifyVmStopped,
                     weak_ptr_factory_.GetWeakPtr(), std::move(signal)));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), stop_vm_response_));
}

void FakeConciergeClient::SuspendVm(
    const vm_tools::concierge::SuspendVmRequest& request,
    DBusMethodCallback<vm_tools::concierge::SuspendVmResponse> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), suspend_vm_response_));
}

void FakeConciergeClient::ResumeVm(
    const vm_tools::concierge::ResumeVmRequest& request,
    DBusMethodCallback<vm_tools::concierge::ResumeVmResponse> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), resume_vm_response_));
}

void FakeConciergeClient::GetVmInfo(
    const vm_tools::concierge::GetVmInfoRequest& request,
    DBusMethodCallback<vm_tools::concierge::GetVmInfoResponse> callback) {
  get_vm_info_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), get_vm_info_response_));
}

void FakeConciergeClient::GetVmEnterpriseReportingInfo(
    const vm_tools::concierge::GetVmEnterpriseReportingInfoRequest& request,
    DBusMethodCallback<
        vm_tools::concierge::GetVmEnterpriseReportingInfoResponse> callback) {
  get_vm_enterprise_reporting_info_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                get_vm_enterprise_reporting_info_response_));
}

void FakeConciergeClient::SetVmCpuRestriction(
    const vm_tools::concierge::SetVmCpuRestrictionRequest& request,
    DBusMethodCallback<vm_tools::concierge::SetVmCpuRestrictionResponse>
        callback) {
  set_vm_cpu_restriction_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), set_vm_cpu_restriction_response_));
}

void FakeConciergeClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  wait_for_service_to_be_available_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                wait_for_service_to_be_available_response_));
}

void FakeConciergeClient::GetContainerSshKeys(
    const vm_tools::concierge::ContainerSshKeysRequest& request,
    DBusMethodCallback<vm_tools::concierge::ContainerSshKeysResponse>
        callback) {
  get_container_ssh_keys_called_ = true;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), container_ssh_keys_response_));
}

void FakeConciergeClient::AttachUsbDevice(
    base::ScopedFD fd,
    const vm_tools::concierge::AttachUsbDeviceRequest& request,
    DBusMethodCallback<vm_tools::concierge::AttachUsbDeviceResponse> callback) {
  attach_usb_device_called_ = true;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), attach_usb_device_response_));
}

void FakeConciergeClient::DetachUsbDevice(
    const vm_tools::concierge::DetachUsbDeviceRequest& request,
    DBusMethodCallback<vm_tools::concierge::DetachUsbDeviceResponse> callback) {
  detach_usb_device_called_ = true;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), detach_usb_device_response_));
}

void FakeConciergeClient::StartArcVm(
    const vm_tools::concierge::StartArcVmRequest& request,
    DBusMethodCallback<vm_tools::concierge::StartVmResponse> callback) {
  start_arc_vm_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), start_vm_response_));
}

void FakeConciergeClient::ResizeDiskImage(
    const vm_tools::concierge::ResizeDiskImageRequest& request,
    DBusMethodCallback<vm_tools::concierge::ResizeDiskImageResponse> callback) {
  resize_disk_image_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), resize_disk_image_response_));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeConciergeClient::NotifyDiskImageProgress,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FakeConciergeClient::SetVmId(
    const vm_tools::concierge::SetVmIdRequest& request,
    DBusMethodCallback<vm_tools::concierge::SetVmIdResponse> callback) {
  set_vm_id_called_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), set_vm_id_response_));
}

void FakeConciergeClient::NotifyVmStarted(
    const vm_tools::concierge::VmStartedSignal& signal) {
  for (auto& observer : vm_observer_list_)
    observer.OnVmStarted(signal);
}

void FakeConciergeClient::NotifyVmStopped(
    const vm_tools::concierge::VmStoppedSignal& signal) {
  for (auto& observer : vm_observer_list_)
    observer.OnVmStopped(signal);
}

bool FakeConciergeClient::HasVmObservers() const {
  return vm_observer_list_.might_have_observers();
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
  cancel_disk_image_response_.emplace();
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
  get_vm_info_response_->set_success(true);
  get_vm_info_response_->mutable_vm_info()->set_seneschal_server_handle(1);

  get_vm_enterprise_reporting_info_response_.emplace();
  set_vm_cpu_restriction_response_.emplace();

  container_ssh_keys_response_.emplace();
  container_ssh_keys_response_->set_container_public_key("pubkey");
  container_ssh_keys_response_->set_host_private_key("privkey");
  container_ssh_keys_response_->set_hostname("hostname");

  attach_usb_device_response_.emplace();
  attach_usb_device_response_->set_success(true);
  attach_usb_device_response_->set_guest_port(0);

  detach_usb_device_response_.emplace();
  detach_usb_device_response_->set_success(true);

  set_vm_id_response_.emplace();
  set_vm_id_response_->set_success(true);
}

}  // namespace chromeos
