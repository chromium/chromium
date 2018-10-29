// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_CICERONE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_CICERONE_CLIENT_H_

#include "base/observer_list.h"
#include "chromeos/dbus/cicerone_client.h"

namespace chromeos {

// FakeCiceroneClient is a fake implementation of CiceroneClient used for
// testing.
class CHROMEOS_EXPORT FakeCiceroneClient : public CiceroneClient {
 public:
  FakeCiceroneClient();
  ~FakeCiceroneClient() override;

  // CiceroneClient overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // IsContainerStartedSignalConnected must return true before StartContainer
  // is called.
  bool IsContainerStartedSignalConnected() override;

  // IsContainerShutdownSignalConnected must return true before StartContainer
  // is called.
  bool IsContainerShutdownSignalConnected() override;

  // This should be true prior to calling InstallLinuxPackage.
  bool IsInstallLinuxPackageProgressSignalConnected() override;

  // This should be true prior to calling CreateLxdContainer or
  // StartLxdContainer.
  bool IsLxdContainerCreatedSignalConnected() override;

  // This should be true prior to calling CreateLxdContainer or
  // StartLxdContainer.
  bool IsLxdContainerDownloadingSignalConnected() override;

  // This should be true prior to calling CreateLxdContainer or
  // StartLxdContainer.
  bool IsTremplinStartedSignalConnected() override;

  // Fake version of the method that launches an application inside a running
  // Container. |callback| is called after the method call finishes.
  void LaunchContainerApplication(
      const vm_tools::cicerone::LaunchContainerApplicationRequest& request,
      DBusMethodCallback<vm_tools::cicerone::LaunchContainerApplicationResponse>
          callback) override;

  // Fake version of the method that gets application icons from inside a
  // Container. |callback| is called after the method call finishes.
  void GetContainerAppIcons(
      const vm_tools::cicerone::ContainerAppIconRequest& request,
      DBusMethodCallback<vm_tools::cicerone::ContainerAppIconResponse> callback)
      override;

  // Fake version of the method that gets information about a Linux package file
  // inside a Container. |callback| is called after the method call finishes.
  void GetLinuxPackageInfo(
      const vm_tools::cicerone::LinuxPackageInfoRequest& request,
      DBusMethodCallback<vm_tools::cicerone::LinuxPackageInfoResponse> callback)
      override;

  // Fake version of the method that installs an application inside a running
  // Container. |callback| is called after the method call finishes. This does
  // not cause progress events to be fired.
  void InstallLinuxPackage(
      const vm_tools::cicerone::InstallLinuxPackageRequest& request,
      DBusMethodCallback<vm_tools::cicerone::InstallLinuxPackageResponse>
          callback) override;

  // Fake version of the method that creates a new Container.
  // |callback| is called to indicate creation status.
  void CreateLxdContainer(
      const vm_tools::cicerone::CreateLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::CreateLxdContainerResponse>
          callback) override;

  // Fake version of the method that starts a new Container.
  // |callback| is called when the method completes.
  void StartLxdContainer(
      const vm_tools::cicerone::StartLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::StartLxdContainerResponse>
          callback) override;

  // Fake version of the method that gets the container username.
  // |callback| is called when the method completes.
  void GetLxdContainerUsername(
      const vm_tools::cicerone::GetLxdContainerUsernameRequest& request,
      DBusMethodCallback<vm_tools::cicerone::GetLxdContainerUsernameResponse>
          callback) override;

  // Fake version of the method that sets the container user.
  // |callback| is called when the method completes.
  void SetUpLxdContainerUser(
      const vm_tools::cicerone::SetUpLxdContainerUserRequest& request,
      DBusMethodCallback<vm_tools::cicerone::SetUpLxdContainerUserResponse>
          callback) override;

  // Fake version of the method that waits for the Cicerone service to be
  // availble.  |callback| is called after the method call finishes.
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

  // Set ContainerStartedSignalConnected state
  void set_container_started_signal_connected(bool connected) {
    is_container_started_signal_connected_ = connected;
  }

  // Set ContainerShutdownSignalConnected state
  void set_container_shutdown_signal_connected(bool connected) {
    is_container_shutdown_signal_connected_ = connected;
  }

  // Set InstallLinuxPackageProgressSignalConnected state
  void set_install_linux_package_progress_signal_connected(bool connected) {
    is_install_linux_package_progress_signal_connected_ = connected;
  }

  // Set LxdContainerCreatedSignalConnected state
  void set_lxd_container_created_signal_connected(bool connected) {
    is_lxd_container_created_signal_connected_ = connected;
  }

  // Set LxdContainerCreatedSignalConnected response status
  void set_lxd_container_created_signal_status(
      vm_tools::cicerone::LxdContainerCreatedSignal_Status status) {
    lxd_container_created_signal_status_ = status;
  }
  // Set LxdContainerDownloadingSignalConnected state
  void set_lxd_container_downloading_signal_connected(bool connected) {
    is_lxd_container_downloading_signal_connected_ = connected;
  }
  // Set TremplinStartedSignalConnected state
  void set_tremplin_started_signal_connected(bool connected) {
    is_tremplin_started_signal_connected_ = connected;
  }

  void set_launch_container_application_response(
      const vm_tools::cicerone::LaunchContainerApplicationResponse&
          launch_container_application_response) {
    launch_container_application_response_ =
        launch_container_application_response;
  }

  void set_container_app_icon_response(
      const vm_tools::cicerone::ContainerAppIconResponse&
          container_app_icon_response) {
    container_app_icon_response_ = container_app_icon_response;
  }

  void set_linux_package_info_response(
      const vm_tools::cicerone::LinuxPackageInfoResponse&
          get_linux_package_info_response) {
    get_linux_package_info_response_ = get_linux_package_info_response;
  }

  void set_install_linux_package_response(
      const vm_tools::cicerone::InstallLinuxPackageResponse&
          install_linux_package_response) {
    install_linux_package_response_ = install_linux_package_response;
  }

  void set_create_lxd_container_response(
      const vm_tools::cicerone::CreateLxdContainerResponse&
          create_lxd_container_response) {
    create_lxd_container_response_ = create_lxd_container_response;
  }

  void set_start_lxd_container_response(
      const vm_tools::cicerone::StartLxdContainerResponse&
          start_lxd_container_response) {
    start_lxd_container_response_ = start_lxd_container_response;
  }

  void set_get_lxd_container_username_response(
      const vm_tools::cicerone::GetLxdContainerUsernameResponse&
          get_lxd_container_username_response) {
    get_lxd_container_username_response_ = get_lxd_container_username_response;
  }

  void set_setup_lxd_container_user_response(
      const vm_tools::cicerone::SetUpLxdContainerUserResponse&
          setup_lxd_container_user_response) {
    setup_lxd_container_user_response_ = setup_lxd_container_user_response;
  }

  // Additional functions to allow tests to trigger Signals.
  void NotifyLxdContainerCreated(
      const vm_tools::cicerone::LxdContainerCreatedSignal& signal);
  void NotifyContainerStarted(
      const vm_tools::cicerone::ContainerStartedSignal& signal);
  void NotifyTremplinStarted(
      const vm_tools::cicerone::TremplinStartedSignal& signal);

 protected:
  void Init(dbus::Bus* bus) override {}

 private:
  bool is_container_started_signal_connected_ = true;
  bool is_container_shutdown_signal_connected_ = true;
  bool is_install_linux_package_progress_signal_connected_ = true;
  bool is_lxd_container_created_signal_connected_ = true;
  bool is_lxd_container_downloading_signal_connected_ = true;
  bool is_tremplin_started_signal_connected_ = true;

  vm_tools::cicerone::LxdContainerCreatedSignal_Status
      lxd_container_created_signal_status_ =
          vm_tools::cicerone::LxdContainerCreatedSignal::CREATED;

  vm_tools::cicerone::LaunchContainerApplicationResponse
      launch_container_application_response_;
  vm_tools::cicerone::ContainerAppIconResponse container_app_icon_response_;
  vm_tools::cicerone::LinuxPackageInfoResponse get_linux_package_info_response_;
  vm_tools::cicerone::InstallLinuxPackageResponse
      install_linux_package_response_;
  vm_tools::cicerone::CreateLxdContainerResponse create_lxd_container_response_;
  vm_tools::cicerone::StartLxdContainerResponse start_lxd_container_response_;
  vm_tools::cicerone::GetLxdContainerUsernameResponse
      get_lxd_container_username_response_;
  vm_tools::cicerone::SetUpLxdContainerUserResponse
      setup_lxd_container_user_response_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(FakeCiceroneClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_CICERONE_CLIENT_H_
