// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

namespace {

FakeCiceroneClient* g_instance = nullptr;

}  // namespace

FakeCiceroneClient::FakeCiceroneClient() {
  DCHECK(!g_instance);
  g_instance = this;

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

  stop_lxd_container_response_.set_status(
      vm_tools::cicerone::StopLxdContainerResponse::STOPPING);

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

  start_lxd_response_.set_status(
      vm_tools::cicerone::StartLxdResponse::ALREADY_RUNNING);

  add_file_watch_response_.set_status(
      vm_tools::cicerone::AddFileWatchResponse::SUCCEEDED);

  remove_file_watch_response_.set_status(
      vm_tools::cicerone::RemoveFileWatchResponse::SUCCEEDED);

  attach_usb_to_container_response_.set_status(
      vm_tools::cicerone::AttachUsbToContainerResponse::OK);

  detach_usb_from_container_response_.set_status(
      vm_tools::cicerone::DetachUsbFromContainerResponse::OK);

  update_container_devices_response_.set_status(
      vm_tools::cicerone::UpdateContainerDevicesResponse::OK);
}

FakeCiceroneClient::~FakeCiceroneClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void FakeCiceroneClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeCiceroneClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeCiceroneClient::NotifyCiceroneStopped() {
  for (auto& observer : observer_list_) {
    observer.CiceroneServiceStopped();
  }
}
void FakeCiceroneClient::NotifyCiceroneStarted() {
  for (auto& observer : observer_list_) {
    observer.CiceroneServiceStarted();
  }
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

bool FakeCiceroneClient::IsStartLxdProgressSignalConnected() {
  return is_start_lxd_progress_signal_connected_;
}

bool FakeCiceroneClient::IsFileWatchTriggeredSignalConnected() {
  return is_file_watch_triggered_signal_connected_;
}

bool FakeCiceroneClient::IsLowDiskSpaceTriggeredSignalConnected() {
  return is_low_disk_space_triggered_signal_connected_;
}

bool FakeCiceroneClient::IsInhibitScreensaverSignalConencted() {
  return is_inhibit_screensaver_signal_connected_;
}

bool FakeCiceroneClient::IsUninhibitScreensaverSignalConencted() {
  return is_uninhibit_screensaver_signal_connected_;
}

// Currently no tests need to change the output of this method. If you want to
// add one, make it return a variable like the above examples.
bool FakeCiceroneClient::IsPendingAppListUpdatesSignalConnected() {
  return true;
}

void FakeCiceroneClient::LaunchContainerApplication(
    const vm_tools::cicerone::LaunchContainerApplicationRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::LaunchContainerApplicationResponse> callback) {
  if (launch_container_application_callback_) {
    launch_container_application_callback_.Run(request, std::move(callback));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  launch_container_application_response_));
  }
}

void FakeCiceroneClient::GetContainerAppIcons(
    const vm_tools::cicerone::ContainerAppIconRequest& request,
    chromeos::DBusMethodCallback<vm_tools::cicerone::ContainerAppIconResponse>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), container_app_icon_response_));
}

void FakeCiceroneClient::GetLinuxPackageInfo(
    const vm_tools::cicerone::LinuxPackageInfoRequest& request,
    chromeos::DBusMethodCallback<vm_tools::cicerone::LinuxPackageInfoResponse>
        callback) {
  most_recent_linux_package_info_request_ = request;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), get_linux_package_info_response_));
}

void FakeCiceroneClient::InstallLinuxPackage(
    const vm_tools::cicerone::InstallLinuxPackageRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::InstallLinuxPackageResponse> callback) {
  most_recent_install_linux_package_request_ = request;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), install_linux_package_response_));
}

void FakeCiceroneClient::SetOnLaunchContainerApplicationCallback(
    LaunchContainerApplicationCallback callback) {
  launch_container_application_callback_ = std::move(callback);
}

void FakeCiceroneClient::SetOnUninstallPackageOwningFileCallback(
    UninstallPackageOwningFileCallback callback) {
  uninstall_package_owning_file_callback_ = std::move(callback);
}

void FakeCiceroneClient::UninstallPackageOwningFile(
    const vm_tools::cicerone::UninstallPackageOwningFileRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::UninstallPackageOwningFileResponse> callback) {
  if (uninstall_package_owning_file_callback_) {
    uninstall_package_owning_file_callback_.Run(request, std::move(callback));
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  uninstall_package_owning_file_response_));
  }
}

void FakeCiceroneClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeCiceroneClient::CreateLxdContainer(
    const vm_tools::cicerone::CreateLxdContainerRequest& request,
    chromeos::DBusMethodCallback<vm_tools::cicerone::CreateLxdContainerResponse>
        callback) {
  create_lxd_container_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), create_lxd_container_response_),
      send_create_lxd_container_response_delay_);

  // Trigger CiceroneClient::Observer::NotifyLxdContainerCreatedSignal.
  vm_tools::cicerone::LxdContainerCreatedSignal signal;
  signal.set_owner_id(request.owner_id());
  signal.set_vm_name(request.vm_name());
  signal.set_container_name(request.container_name());
  signal.set_status(lxd_container_created_signal_status_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeCiceroneClient::NotifyLxdContainerCreated,
                     weak_factory_.GetWeakPtr(), std::move(signal)),
      send_notify_lxd_container_created_signal_delay_);
}

void FakeCiceroneClient::DeleteLxdContainer(
    const vm_tools::cicerone::DeleteLxdContainerRequest& request,
    chromeos::DBusMethodCallback<vm_tools::cicerone::DeleteLxdContainerResponse>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), delete_lxd_container_response_));
}

void FakeCiceroneClient::StartLxdContainer(
    const vm_tools::cicerone::StartLxdContainerRequest& request,
    chromeos::DBusMethodCallback<vm_tools::cicerone::StartLxdContainerResponse>
        callback) {
  start_lxd_container_count_++;
  start_lxd_container_response_.mutable_os_release()->CopyFrom(
      lxd_container_os_release_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), start_lxd_container_response_),
      send_start_lxd_container_response_delay_);

  // Trigger CiceroneClient::Observer::NotifyLxdContainerStartingSignal.
  vm_tools::cicerone::LxdContainerStartingSignal starting_signal;
  starting_signal.set_owner_id(request.owner_id());
  starting_signal.set_vm_name(request.vm_name());
  starting_signal.set_container_name(request.container_name());
  starting_signal.set_status(lxd_container_starting_signal_status_);
  starting_signal.mutable_os_release()->CopyFrom(lxd_container_os_release_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeCiceroneClient::NotifyLxdContainerStarting,
                     weak_factory_.GetWeakPtr(), std::move(starting_signal)),
      send_container_starting_signal_delay_);

  // Trigger CiceroneClient::Observer::NotifyContainerStartedSignal.
  vm_tools::cicerone::ContainerStartedSignal started_signal;
  started_signal.set_owner_id(request.owner_id());
  started_signal.set_vm_name(request.vm_name());
  started_signal.set_container_name(request.container_name());
  started_signal.set_container_username(last_container_username_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeCiceroneClient::NotifyContainerStarted,
                     weak_factory_.GetWeakPtr(), std::move(started_signal)),
      send_container_started_signal_delay_);
}

void FakeCiceroneClient::StopLxdContainer(
    const vm_tools::cicerone::StopLxdContainerRequest& request,
    chromeos::DBusMethodCallback<vm_tools::cicerone::StopLxdContainerResponse>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), stop_lxd_container_response_),
      send_stop_lxd_container_response_delay_);

  // Trigger CiceroneClient::Observer::NotifyContainerShutdownSignal
  vm_tools::cicerone::ContainerShutdownSignal shutdown_signal;
  shutdown_signal.set_owner_id(request.owner_id());
  shutdown_signal.set_vm_name(request.vm_name());
  shutdown_signal.set_container_name(request.container_name());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeCiceroneClient::NotifyContainerShutdownSignal,
                     weak_factory_.GetWeakPtr(), std::move(shutdown_signal)),
      send_stop_lxd_container_response_delay_);
}

void FakeCiceroneClient::GetLxdContainerUsername(
    const vm_tools::cicerone::GetLxdContainerUsernameRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::GetLxdContainerUsernameResponse> callback) {
  last_container_username_ = get_lxd_container_username_response_.username();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                get_lxd_container_username_response_));
}

void FakeCiceroneClient::SetUpLxdContainerUser(
    const vm_tools::cicerone::SetUpLxdContainerUserRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::SetUpLxdContainerUserResponse> callback) {
  setup_lxd_container_user_count_++;
  setup_lxd_container_user_request_ = request;
  last_container_username_ = request.container_username();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), setup_lxd_container_user_response_),
      send_set_up_lxd_container_user_response_delay_);
}

void FakeCiceroneClient::ExportLxdContainer(
    const vm_tools::cicerone::ExportLxdContainerRequest& request,
    chromeos::DBusMethodCallback<vm_tools::cicerone::ExportLxdContainerResponse>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), export_lxd_container_response_));
}

void FakeCiceroneClient::ImportLxdContainer(
    const vm_tools::cicerone::ImportLxdContainerRequest& request,
    chromeos::DBusMethodCallback<vm_tools::cicerone::ImportLxdContainerResponse>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), import_lxd_container_response_));
}

void FakeCiceroneClient::CancelExportLxdContainer(
    const vm_tools::cicerone::CancelExportLxdContainerRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::CancelExportLxdContainerResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                cancel_export_lxd_container_response_));
}

void FakeCiceroneClient::CancelImportLxdContainer(
    const vm_tools::cicerone::CancelImportLxdContainerRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::CancelImportLxdContainerResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                cancel_import_lxd_container_response_));
}

void FakeCiceroneClient::ApplyAnsiblePlaybook(
    const vm_tools::cicerone::ApplyAnsiblePlaybookRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::ApplyAnsiblePlaybookResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), apply_ansible_playbook_response_));
}

void FakeCiceroneClient::ConfigureForArcSideload(
    const vm_tools::cicerone::ConfigureForArcSideloadRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::ConfigureForArcSideloadResponse> callback) {
  configure_for_arc_sideload_called_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), enable_arc_sideload_response_));
}

void FakeCiceroneClient::UpgradeContainer(
    const vm_tools::cicerone::UpgradeContainerRequest& request,
    chromeos::DBusMethodCallback<vm_tools::cicerone::UpgradeContainerResponse>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), upgrade_container_response_));
}

void FakeCiceroneClient::CancelUpgradeContainer(
    const vm_tools::cicerone::CancelUpgradeContainerRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::CancelUpgradeContainerResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), cancel_upgrade_container_response_));
}

void FakeCiceroneClient::StartLxd(
    const vm_tools::cicerone::StartLxdRequest& request,
    chromeos::DBusMethodCallback<vm_tools::cicerone::StartLxdResponse>
        callback) {
  start_lxd_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), start_lxd_response_),
      send_start_lxd_response_delay_);
}

void FakeCiceroneClient::AddFileWatch(
    const vm_tools::cicerone::AddFileWatchRequest& request,
    chromeos::DBusMethodCallback<vm_tools::cicerone::AddFileWatchResponse>
        callback) {
  add_file_watch_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), add_file_watch_response_));
}

void FakeCiceroneClient::RemoveFileWatch(
    const vm_tools::cicerone::RemoveFileWatchRequest& request,
    chromeos::DBusMethodCallback<vm_tools::cicerone::RemoveFileWatchResponse>
        callback) {
  remove_file_watch_call_count_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), remove_file_watch_response_));
}

void FakeCiceroneClient::GetVshSession(
    const vm_tools::cicerone::GetVshSessionRequest& request,
    chromeos::DBusMethodCallback<vm_tools::cicerone::GetVshSessionResponse>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), get_vsh_session_response_));
}

void FakeCiceroneClient::AttachUsbToContainer(
    const vm_tools::cicerone::AttachUsbToContainerRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::AttachUsbToContainerResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), attach_usb_to_container_response_));
}

void FakeCiceroneClient::DetachUsbFromContainer(
    const vm_tools::cicerone::DetachUsbFromContainerRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::DetachUsbFromContainerResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), detach_usb_from_container_response_));
}

void FakeCiceroneClient::FileSelected(
    const vm_tools::cicerone::FileSelectedSignal& signal) {}

void FakeCiceroneClient::ListRunningContainers(
    const vm_tools::cicerone::ListRunningContainersRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::ListRunningContainersResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), list_containers_response_));
}

void FakeCiceroneClient::GetGarconSessionInfo(
    const vm_tools::cicerone::GetGarconSessionInfoRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::GetGarconSessionInfoResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), get_garcon_session_info_response_));
}

void FakeCiceroneClient::UpdateContainerDevices(
    const vm_tools::cicerone::UpdateContainerDevicesRequest& request,
    chromeos::DBusMethodCallback<
        vm_tools::cicerone::UpdateContainerDevicesResponse> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), update_container_devices_response_));
}

void FakeCiceroneClient::NotifyLxdContainerCreated(
    const vm_tools::cicerone::LxdContainerCreatedSignal& proto) {
  for (auto& observer : observer_list_) {
    observer.OnLxdContainerCreated(proto);
  }
}

void FakeCiceroneClient::NotifyLxdContainerDeleted(
    const vm_tools::cicerone::LxdContainerDeletedSignal& proto) {
  for (auto& observer : observer_list_) {
    observer.OnLxdContainerDeleted(proto);
  }
}

void FakeCiceroneClient::NotifyContainerStarted(
    const vm_tools::cicerone::ContainerStartedSignal& proto) {
  for (auto& observer : observer_list_) {
    observer.OnContainerStarted(proto);
  }
}

void FakeCiceroneClient::NotifyContainerShutdownSignal(
    const vm_tools::cicerone::ContainerShutdownSignal& proto) {
  for (auto& observer : observer_list_) {
    observer.OnContainerShutdown(proto);
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

void FakeCiceroneClient::NotifyStartLxdProgress(
    const vm_tools::cicerone::StartLxdProgressSignal& signal) {
  for (auto& observer : observer_list_) {
    observer.OnStartLxdProgress(signal);
  }
}

void FakeCiceroneClient::NotifyFileWatchTriggered(
    const vm_tools::cicerone::FileWatchTriggeredSignal& signal) {
  for (auto& observer : observer_list_) {
    observer.OnFileWatchTriggered(signal);
  }
}

// static
FakeCiceroneClient* FakeCiceroneClient::Get() {
  return g_instance;
}

}  // namespace ash
