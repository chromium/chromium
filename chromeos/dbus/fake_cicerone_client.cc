// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cicerone_client.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeCiceroneClient::FakeCiceroneClient() {
  launch_container_application_response_.Clear();
  launch_container_application_response_.set_success(true);

  container_app_icon_response_.Clear();

  get_linux_package_info_response_.Clear();
  get_linux_package_info_response_.set_success(true);
  get_linux_package_info_response_.set_package_id("Fake Package;1.0;x86-64");
  get_linux_package_info_response_.set_summary("A package that is fake");

  install_linux_package_response_.Clear();
  install_linux_package_response_.set_status(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);

  create_lxd_container_response_.Clear();
  create_lxd_container_response_.set_status(
      vm_tools::cicerone::CreateLxdContainerResponse::CREATING);

  start_lxd_container_response_.Clear();
  start_lxd_container_response_.set_status(
      vm_tools::cicerone::StartLxdContainerResponse::STARTED);

  setup_lxd_container_user_response_.Clear();
  setup_lxd_container_user_response_.set_status(
      vm_tools::cicerone::SetUpLxdContainerUserResponse::SUCCESS);
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

bool FakeCiceroneClient::IsLxdContainerDownloadingSignalConnected() {
  return is_lxd_container_downloading_signal_connected_;
}

bool FakeCiceroneClient::IsTremplinStartedSignalConnected() {
  return is_tremplin_started_signal_connected_;
}

bool FakeCiceroneClient::IsInstallLinuxPackageProgressSignalConnected() {
  return is_install_linux_package_progress_signal_connected_;
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), get_linux_package_info_response_));
}

void FakeCiceroneClient::InstallLinuxPackage(
    const vm_tools::cicerone::InstallLinuxPackageRequest& request,
    DBusMethodCallback<vm_tools::cicerone::InstallLinuxPackageResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), install_linux_package_response_));
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

void FakeCiceroneClient::StartLxdContainer(
    const vm_tools::cicerone::StartLxdContainerRequest& request,
    DBusMethodCallback<vm_tools::cicerone::StartLxdContainerResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), start_lxd_container_response_));
}

void FakeCiceroneClient::GetLxdContainerUsername(
    const vm_tools::cicerone::GetLxdContainerUsernameRequest& request,
    DBusMethodCallback<vm_tools::cicerone::GetLxdContainerUsernameResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                get_lxd_container_username_response_));
}

void FakeCiceroneClient::SetUpLxdContainerUser(
    const vm_tools::cicerone::SetUpLxdContainerUserRequest& request,
    DBusMethodCallback<vm_tools::cicerone::SetUpLxdContainerUserResponse>
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), setup_lxd_container_user_response_));

  // Trigger CiceroneClient::Observer::NotifyContainerStartedSignal.
  vm_tools::cicerone::ContainerStartedSignal signal;
  signal.set_owner_id(request.owner_id());
  signal.set_vm_name(request.vm_name());
  signal.set_container_name(request.container_name());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeCiceroneClient::NotifyContainerStarted,
                                base::Unretained(this), std::move(signal)));
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

}  // namespace chromeos
