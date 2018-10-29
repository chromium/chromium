// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_concierge_client.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"

namespace chromeos {

FakeConciergeClient::FakeConciergeClient() : weak_ptr_factory_(this) {
  InitializeProtoResponses();
}
FakeConciergeClient::~FakeConciergeClient() = default;

// ConciergeClient override.
void FakeConciergeClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

// ConciergeClient override.
void FakeConciergeClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

// ConciergeClient override.
bool FakeConciergeClient::IsContainerStartupFailedSignalConnected() {
  return is_container_startup_failed_signal_connected_;
}

void FakeConciergeClient::CreateDiskImage(
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

  if (start_vm_response_.status() != vm_tools::concierge::VM_STATUS_STARTING) {
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), stop_vm_response_));
}

void FakeConciergeClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
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

void FakeConciergeClient::InitializeProtoResponses() {
  create_disk_image_response_.Clear();
  create_disk_image_response_.set_status(
      vm_tools::concierge::DISK_STATUS_CREATED);
  create_disk_image_response_.set_disk_path("foo");

  destroy_disk_image_response_.Clear();
  destroy_disk_image_response_.set_status(
      vm_tools::concierge::DISK_STATUS_DESTROYED);

  list_vm_disks_response_.Clear();
  list_vm_disks_response_.set_success(true);

  start_vm_response_.Clear();
  start_vm_response_.set_status(vm_tools::concierge::VM_STATUS_STARTING);

  stop_vm_response_.Clear();
  stop_vm_response_.set_success(true);

  container_ssh_keys_response_.Clear();
  container_ssh_keys_response_.set_container_public_key("pubkey");
  container_ssh_keys_response_.set_host_private_key("privkey");
  container_ssh_keys_response_.set_hostname("hostname");
}

}  // namespace chromeos
