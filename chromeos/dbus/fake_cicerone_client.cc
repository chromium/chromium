// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cicerone_client.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeCiceroneClient::FakeCiceroneClient() {
  launch_container_application_response_.set_success(true);

  get_linux_package_info_response_.set_success(true);
  get_linux_package_info_response_.set_package_id("Fake Package;1.0;x86-64");
  get_linux_package_info_response_.set_summary("A package that is fake");

  install_linux_package_response_.set_status(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);

  uninstall_package_owning_file_response_.set_status(
      vm_tools::cicerone::UninstallPackageOwningFileResponse::STARTED);

  create_lxd_container_response_.set_status(
      vm_tools::cicerone::CreateLxdContainerResponse::CREATING);

  start_lxd_container_response_.set_status(
      vm_tools::cicerone::StartLxdContainerResponse::STARTING);

  setup_lxd_container_user_response_.set_status(
      vm_tools::cicerone::SetUpLxdContainerUserResponse::SUCCESS);

  export_lxd_container_response_.set_status(
      vm_tools::cicerone::ExportLxdContainerResponse::EXPORTING);

  import_lxd_container_response_.set_status(
      vm_tools::cicerone::ImportLxdContainerResponse::IMPORTING);

  upgrade_container_response_.set_status(
      vm_tools::cicerone::UpgradeContainerResponse::STARTED);

  cancel_upgrade_container_response_.set_status(
      vm_tools::cicerone::CancelUpgradeContainerResponse::CANCELLED);
}

FakeCiceroneClient::~FakeCiceroneClient() = default;

void FakeCiceroneClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeCiceroneClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool FakeCiceroneClient::IsContainerStartedSignalConnected() {
  return is_container_started_signal_connected_;
}

bool FakeCiceroneClient::IsContainerShutdownSignalConnected() {
  return is_container_shutdown_signal_connected_;
}

bool FakeCiceroneClient::IsLxdContainerCreatedSignalConnected() {
  return is_lxd_container_created_signal_connected_;
}

bool FakeCiceroneClient::IsLxdContainerDeletedSignalConnected() {
  return is_lxd_container_deleted_signal_connected_;
}

bool FakeCiceroneClient::IsLxdContainerDownloadingSignalConnected() {
  return is_lxd_container_downloading_signal_connected_;
}

bool FakeCiceroneClient::IsTremplinStartedSignalConnected() {
  return is_tremplin_started_signal_connected_;
}

bool FakeCiceroneClient::IsLxdContainerStartingSignalConnected() {
  return is_lxd_container_starting_signal_connected_;
}

bool FakeCiceroneClient::IsInstallLinuxPackageProgressSignalConnected() {
  return is_install_linux_package_progress_signal_connected_;
}

bool FakeCiceroneClient::IsUninstallPackageProgressSignalConnected() {
  return is_uninstall_package_progress_signal_connected_;
}

bool FakeCiceroneClient::IsExportLxdContainerProgressSignalConnected() {
  return is_export_lxd_container_progress_signal_connected_;
}

bool FakeCiceroneClient::IsImportLxdContainerProgressSignalConnected() {
  return is_import_lxd_container_progress_signal_connected_;
}

bool FakeCiceroneClient::IsApplyAnsiblePlaybookProgressSignalConnected() {
  return is_apply_ansible_playbook_progress_signal_connected_;
}

bool FakeCiceroneClient::IsUpgradeContainerProgressSignalConnected() {
  return is_upgrade_container_progress_signal_connected_;
}

// Currently no tests need to change the output of this method. If you want to
// add one, make it return a variable like the above examples.
bool FakeCiceroneClient::IsPendingAppListUpdatesSignalConnected() {
  return true;
}

void FakeCiceroneClient::LaunchContainerApplication(
    const vm_tools::cicerone::LaunchContainerApplicationRequest& request,
    DBusMethodCallback<vm_tools::cicerone::LaunchContainerApplicationResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                launch_container_application_response_));
}

void FakeCiceroneClient::GetContainerAppIcons(
    const vm_tools::cicerone::ContainerAppIconRequest& request,
    DBusMethodCallback<vm_tools::cicerone::ContainerAppIconResponse> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), container_app_icon_response_));
}

void FakeCiceroneClient::GetLinuxPackageInfo(
    const vm_tools::cicerone::LinuxPackageInfoRequest& request,
    DBusMethodCallback<vm_tools::cicerone::LinuxPackageInfoResponse> callback) {
  most_recent_linux_package_info_request_ = request;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), get_linux_package_info_response_));
}

void FakeCiceroneClient::InstallLinuxPackage(
    const vm_tools::cicerone::InstallLinuxPackageRequest& request,
    DBusMethodCallback<vm_tools::cicerone::InstallLinuxPackageResponse>
        callback) {
  most_recent_install_linux_package_request_ = request;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), install_linux_package_response_));
}

void FakeCiceroneClient::SetOnUninstallPackageOwningFileCallback(
    UninstallPackageOwningFileCallback callback) {
  uninstall_package_owning_file_callback_ = std::move(callback);
}

void FakeCiceroneClient::UninstallPackageOwningFile(
    const vm_tools::cicerone::UninstallPackageOwningFileRequest& request,
    DBusMethodCallback<vm_tools::cicerone::UninstallPackageOwningFileResponse>
        callback) {
  if (uninstall_package_owning_file_callback_) {
    uninstall_package_owning_file_callback_.Run(request, std::move(callback));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  uninstall_package_owning_file_response_));
  }
}

void FakeCiceroneClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeCiceroneClient::CreateLxdContainer(
    const vm_tools::cicerone::CreateLxdContainerRequest& request,
    DBusMethodCallback<vm_tools::cicerone::CreateLxdContainerResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), create_lxd_container_response_));

  // Trigger CiceroneClient::Observer::NotifyLxdContainerCreatedSignal.
  vm_tools::cicerone::LxdContainerCreatedSignal signal;
  signal.set_owner_id(request.owner_id());
  signal.set_vm_name(request.vm_name());
  signal.set_container_name(request.container_name());
  signal.set_status(lxd_container_created_signal_status_);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeCiceroneClient::NotifyLxdContainerCreated,
                                base::Unretained(this), std::move(signal)));
}

void FakeCiceroneClient::DeleteLxdContainer(
    const vm_tools::cicerone::DeleteLxdContainerRequest& request,
    DBusMethodCallback<vm_tools::cicerone::DeleteLxdContainerResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), delete_lxd_container_response_));
}

void FakeCiceroneClient::StartLxdContainer(
    const vm_tools::cicerone::StartLxdContainerRequest& request,
    DBusMethodCallback<vm_tools::cicerone::StartLxdContainerResponse>
        callback) {
  start_lxd_container_response_.mutable_os_release()->CopyFrom(
      lxd_container_os_release_);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), start_lxd_container_response_));

  // Trigger CiceroneClient::Observer::NotifyLxdContainerStartingSignal.
  vm_tools::cicerone::LxdContainerStartingSignal signal;
  signal.set_owner_id(request.owner_id());
  signal.set_vm_name(request.vm_name());
  signal.set_container_name(request.container_name());
  signal.set_status(lxd_container_starting_signal_status_);
  signal.mutable_os_release()->CopyFrom(lxd_container_os_release_);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeCiceroneClient::NotifyLxdContainerStarting,
                                base::Unretained(this), std::move(signal)));

  if (send_container_started_signal_) {
    // Trigger CiceroneClient::Observer::NotifyContainerStartedSignal.
    vm_tools::cicerone::ContainerStartedSignal signal;
    signal.set_owner_id(request.owner_id());
    signal.set_vm_name(request.vm_name());
    signal.set_container_name(request.container_name());
    signal.set_container_username(last_container_username_);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&FakeCiceroneClient::NotifyContainerStarted,
                                  base::Unretained(this), std::move(signal)));
  }
}

void FakeCiceroneClient::GetLxdContainerUsername(
    const vm_tools::cicerone::GetLxdContainerUsernameRequest& request,
    DBusMethodCallback<vm_tools::cicerone::GetLxdContainerUsernameResponse>
        callback) {
  last_container_username_ = get_lxd_container_username_response_.username();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                get_lxd_container_username_response_));
}

void FakeCiceroneClient::SetUpLxdContainerUser(
    const vm_tools::cicerone::SetUpLxdContainerUserRequest& request,
    DBusMethodCallback<vm_tools::cicerone::SetUpLxdContainerUserResponse>
        callback) {
  last_container_username_ = request.container_username();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), setup_lxd_container_user_response_));
}

void FakeCiceroneClient::ExportLxdContainer(
    const vm_tools::cicerone::ExportLxdContainerRequest& request,
    DBusMethodCallback<vm_tools::cicerone::ExportLxdContainerResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), export_lxd_container_response_));
}

void FakeCiceroneClient::ImportLxdContainer(
    const vm_tools::cicerone::ImportLxdContainerRequest& request,
    DBusMethodCallback<vm_tools::cicerone::ImportLxdContainerResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), import_lxd_container_response_));
}

void FakeCiceroneClient::CancelExportLxdContainer(
    const vm_tools::cicerone::CancelExportLxdContainerRequest& request,
    DBusMethodCallback<vm_tools::cicerone::CancelExportLxdContainerResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                cancel_export_lxd_container_response_));
}

void FakeCiceroneClient::CancelImportLxdContainer(
    const vm_tools::cicerone::CancelImportLxdContainerRequest& request,
    DBusMethodCallback<vm_tools::cicerone::CancelImportLxdContainerResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                cancel_import_lxd_container_response_));
}

void FakeCiceroneClient::ApplyAnsiblePlaybook(
    const vm_tools::cicerone::ApplyAnsiblePlaybookRequest& request,
    DBusMethodCallback<vm_tools::cicerone::ApplyAnsiblePlaybookResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), apply_ansible_playbook_response_));
}

void FakeCiceroneClient::UpgradeContainer(
    const vm_tools::cicerone::UpgradeContainerRequest& request,
    DBusMethodCallback<vm_tools::cicerone::UpgradeContainerResponse> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), upgrade_container_response_));
}

void FakeCiceroneClient::CancelUpgradeContainer(
    const vm_tools::cicerone::CancelUpgradeContainerRequest& request,
    DBusMethodCallback<vm_tools::cicerone::CancelUpgradeContainerResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), cancel_upgrade_container_response_));
}

void FakeCiceroneClient::NotifyLxdContainerCreated(
    const vm_tools::cicerone::LxdContainerCreatedSignal& proto) {
  for (auto& observer : observer_list_) {
    observer.OnLxdContainerCreated(proto);
  }
}

void FakeCiceroneClient::NotifyContainerStarted(
    const vm_tools::cicerone::ContainerStartedSignal& proto) {
  for (auto& observer : observer_list_) {
    observer.OnContainerStarted(proto);
  }
}

void FakeCiceroneClient::NotifyTremplinStarted(
    const vm_tools::cicerone::TremplinStartedSignal& proto) {
  for (auto& observer : observer_list_) {
    observer.OnTremplinStarted(proto);
  }
}

void FakeCiceroneClient::NotifyLxdContainerStarting(
    const vm_tools::cicerone::LxdContainerStartingSignal& proto) {
  for (auto& observer : observer_list_) {
    observer.OnLxdContainerStarting(proto);
  }
}

void FakeCiceroneClient::NotifyExportLxdContainerProgress(
    const vm_tools::cicerone::ExportLxdContainerProgressSignal& proto) {
  for (auto& observer : observer_list_) {
    observer.OnExportLxdContainerProgress(proto);
  }
}

void FakeCiceroneClient::NotifyImportLxdContainerProgress(
    const vm_tools::cicerone::ImportLxdContainerProgressSignal& proto) {
  for (auto& observer : observer_list_) {
    observer.OnImportLxdContainerProgress(proto);
  }
}

void FakeCiceroneClient::InstallLinuxPackageProgress(
    const vm_tools::cicerone::InstallLinuxPackageProgressSignal& signal) {
  for (auto& observer : observer_list_) {
    observer.OnInstallLinuxPackageProgress(signal);
  }
}

void FakeCiceroneClient::UninstallPackageProgress(
    const vm_tools::cicerone::UninstallPackageProgressSignal& signal) {
  for (auto& observer : observer_list_) {
    observer.OnUninstallPackageProgress(signal);
  }
}

void FakeCiceroneClient::NotifyPendingAppListUpdates(
    const vm_tools::cicerone::PendingAppListUpdatesSignal& proto) {
  for (auto& observer : observer_list_) {
    observer.OnPendingAppListUpdates(proto);
  }
}

void FakeCiceroneClient::NotifyApplyAnsiblePlaybookProgress(
    const vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal& signal) {
  for (auto& observer : observer_list_) {
    observer.OnApplyAnsiblePlaybookProgress(signal);
  }
}

void FakeCiceroneClient::NotifyUpgradeContainerProgress(
    const vm_tools::cicerone::UpgradeContainerProgressSignal& signal) {
  for (auto& observer : observer_list_) {
    observer.OnUpgradeContainerProgress(signal);
  }
}

}  // namespace chromeos
