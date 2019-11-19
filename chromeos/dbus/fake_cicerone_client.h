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
class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeCiceroneClient
    : public CiceroneClient {
 public:
  using UninstallPackageOwningFileCallback = base::RepeatingCallback<void(
      const vm_tools::cicerone::UninstallPackageOwningFileRequest&,
      DBusMethodCallback<
          vm_tools::cicerone::UninstallPackageOwningFileResponse>)>;
  FakeCiceroneClient();
  ~FakeCiceroneClient() override;

  // CiceroneClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool IsContainerStartedSignalConnected() override;
  bool IsContainerShutdownSignalConnected() override;
  bool IsInstallLinuxPackageProgressSignalConnected() override;
  bool IsUninstallPackageProgressSignalConnected() override;
  bool IsLxdContainerCreatedSignalConnected() override;
  bool IsLxdContainerDeletedSignalConnected() override;
  bool IsLxdContainerDownloadingSignalConnected() override;
  bool IsTremplinStartedSignalConnected() override;
  bool IsLxdContainerStartingSignalConnected() override;
  bool IsExportLxdContainerProgressSignalConnected() override;
  bool IsImportLxdContainerProgressSignalConnected() override;
  bool IsPendingAppListUpdatesSignalConnected() override;
  bool IsApplyAnsiblePlaybookProgressSignalConnected() override;
  bool IsUpgradeContainerProgressSignalConnected() override;
  void LaunchContainerApplication(
      const vm_tools::cicerone::LaunchContainerApplicationRequest& request,
      DBusMethodCallback<vm_tools::cicerone::LaunchContainerApplicationResponse>
          callback) override;
  void GetContainerAppIcons(
      const vm_tools::cicerone::ContainerAppIconRequest& request,
      DBusMethodCallback<vm_tools::cicerone::ContainerAppIconResponse> callback)
      override;
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
  // If SetOnUninstallPackageOwningFileCallback has been called, it
  // just triggers that callback. Otherwise, it generates a task to call
  // |callback| with the response from
  // set_uninstall_package_owning_file_response.
  void UninstallPackageOwningFile(
      const vm_tools::cicerone::UninstallPackageOwningFileRequest& request,
      DBusMethodCallback<vm_tools::cicerone::UninstallPackageOwningFileResponse>
          callback) override;
  void CreateLxdContainer(
      const vm_tools::cicerone::CreateLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::CreateLxdContainerResponse>
          callback) override;
  void DeleteLxdContainer(
      const vm_tools::cicerone::DeleteLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::DeleteLxdContainerResponse>
          callback) override;
  void StartLxdContainer(
      const vm_tools::cicerone::StartLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::StartLxdContainerResponse>
          callback) override;
  void GetLxdContainerUsername(
      const vm_tools::cicerone::GetLxdContainerUsernameRequest& request,
      DBusMethodCallback<vm_tools::cicerone::GetLxdContainerUsernameResponse>
          callback) override;
  void SetUpLxdContainerUser(
      const vm_tools::cicerone::SetUpLxdContainerUserRequest& request,
      DBusMethodCallback<vm_tools::cicerone::SetUpLxdContainerUserResponse>
          callback) override;
  void ExportLxdContainer(
      const vm_tools::cicerone::ExportLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::ExportLxdContainerResponse>
          callback) override;
  void ImportLxdContainer(
      const vm_tools::cicerone::ImportLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::ImportLxdContainerResponse>
          callback) override;
  void CancelExportLxdContainer(
      const vm_tools::cicerone::CancelExportLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::CancelExportLxdContainerResponse>
          callback) override;
  void CancelImportLxdContainer(
      const vm_tools::cicerone::CancelImportLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::CancelImportLxdContainerResponse>
          callback) override;
  void ApplyAnsiblePlaybook(
      const vm_tools::cicerone::ApplyAnsiblePlaybookRequest& request,
      DBusMethodCallback<vm_tools::cicerone::ApplyAnsiblePlaybookResponse>
          callback) override;
  void UpgradeContainer(
      const vm_tools::cicerone::UpgradeContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::UpgradeContainerResponse> callback)
      override;
  void CancelUpgradeContainer(
      const vm_tools::cicerone::CancelUpgradeContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::CancelUpgradeContainerResponse>
          callback) override;
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

  // Sets a callback to be called during any call to UninstallPackageOwningFile.
  void SetOnUninstallPackageOwningFileCallback(
      UninstallPackageOwningFileCallback callback);
  void set_container_started_signal_connected(bool connected) {
    is_container_started_signal_connected_ = connected;
  }
  void set_container_shutdown_signal_connected(bool connected) {
    is_container_shutdown_signal_connected_ = connected;
  }
  void set_install_linux_package_progress_signal_connected(bool connected) {
    is_install_linux_package_progress_signal_connected_ = connected;
  }
  void set_uninstall_package_progress_signal_connected(bool connected) {
    is_uninstall_package_progress_signal_connected_ = connected;
  }
  void set_lxd_container_created_signal_connected(bool connected) {
    is_lxd_container_created_signal_connected_ = connected;
  }
  void set_lxd_container_created_signal_status(
      vm_tools::cicerone::LxdContainerCreatedSignal_Status status) {
    lxd_container_created_signal_status_ = status;
  }
  void set_lxd_container_deleted_signal_connected(bool connected) {
    is_lxd_container_deleted_signal_connected_ = connected;
  }
  void set_lxd_container_deleted_signal_status(
      vm_tools::cicerone::LxdContainerDeletedSignal_Status status) {
    lxd_container_deleted_signal_status_ = status;
  }
  void set_lxd_container_downloading_signal_connected(bool connected) {
    is_lxd_container_downloading_signal_connected_ = connected;
  }
  void set_tremplin_started_signal_connected(bool connected) {
    is_tremplin_started_signal_connected_ = connected;
  }
  void set_lxd_container_starting_signal_connected(bool connected) {
    is_lxd_container_starting_signal_connected_ = connected;
  }
  void set_export_lxd_container_progress_signal_connected(bool connected) {
    is_export_lxd_container_progress_signal_connected_ = connected;
  }
  void set_import_lxd_container_progress_signal_connected(bool connected) {
    is_import_lxd_container_progress_signal_connected_ = connected;
  }
  void set_upgrade_container_progress_signal_connected(bool connected) {
    is_upgrade_container_progress_signal_connected_ = connected;
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
  const vm_tools::cicerone::LinuxPackageInfoRequest&
  get_most_recent_linux_package_info_request() const {
    return most_recent_linux_package_info_request_;
  }
  void set_linux_package_info_response(
      const vm_tools::cicerone::LinuxPackageInfoResponse&
          get_linux_package_info_response) {
    get_linux_package_info_response_ = get_linux_package_info_response;
  }
  const vm_tools::cicerone::InstallLinuxPackageRequest&
  get_most_recent_install_linux_package_request() const {
    return most_recent_install_linux_package_request_;
  }
  void set_install_linux_package_response(
      const vm_tools::cicerone::InstallLinuxPackageResponse&
          install_linux_package_response) {
    install_linux_package_response_ = install_linux_package_response;
  }
  void set_uninstall_package_owning_file_response(
      const vm_tools::cicerone::UninstallPackageOwningFileResponse&
          uninstall_package_owning_file_response) {
    uninstall_package_owning_file_response_ =
        uninstall_package_owning_file_response;
  }
  void set_create_lxd_container_response(
      const vm_tools::cicerone::CreateLxdContainerResponse&
          create_lxd_container_response) {
    create_lxd_container_response_ = create_lxd_container_response;
  }
  void set_delete_lxd_container_response_(
      const vm_tools::cicerone::DeleteLxdContainerResponse&
          delete_lxd_container_response) {
    delete_lxd_container_response_ = delete_lxd_container_response;
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
  void set_export_lxd_container_response(
      const vm_tools::cicerone::ExportLxdContainerResponse&
          export_lxd_container_response) {
    export_lxd_container_response_ = export_lxd_container_response;
  }
  void set_import_lxd_container_response(
      const vm_tools::cicerone::ImportLxdContainerResponse&
          import_lxd_container_response) {
    import_lxd_container_response_ = import_lxd_container_response;
  }
  void set_cancel_export_lxd_container_response(
      vm_tools::cicerone::CancelExportLxdContainerResponse
          cancel_export_lxd_container_response) {
    cancel_export_lxd_container_response_ =
        std::move(cancel_export_lxd_container_response);
  }
  void set_cancel_import_lxd_container_response(
      vm_tools::cicerone::CancelImportLxdContainerResponse
          cancel_import_lxd_container_response) {
    cancel_import_lxd_container_response_ =
        std::move(cancel_import_lxd_container_response);
  }
  void set_lxd_container_os_release(vm_tools::cicerone::OsRelease os_release) {
    lxd_container_os_release_ = std::move(os_release);
  }
  void set_send_container_started_signal(bool send) {
    send_container_started_signal_ = send;
  }
  void set_apply_ansible_playbook_response(
      const vm_tools::cicerone::ApplyAnsiblePlaybookResponse&
          apply_ansible_playbook_response) {
    apply_ansible_playbook_response_ = apply_ansible_playbook_response;
  }
  void set_upgrade_container_response(
      vm_tools::cicerone::UpgradeContainerResponse upgrade_container_response) {
    upgrade_container_response_ = std::move(upgrade_container_response);
  }
  void set_cancel_upgrade_container_response(
      vm_tools::cicerone::CancelUpgradeContainerResponse
          cancel_upgrade_container_response) {
    cancel_upgrade_container_response_ =
        std::move(cancel_upgrade_container_response);
  }

  // Additional functions to allow tests to trigger Signals.
  void NotifyLxdContainerCreated(
      const vm_tools::cicerone::LxdContainerCreatedSignal& signal);
  void NotifyLxdContainerDeleted(
      const vm_tools::cicerone::LxdContainerDeletedSignal& signal);
  void NotifyContainerStarted(
      const vm_tools::cicerone::ContainerStartedSignal& signal);
  void NotifyTremplinStarted(
      const vm_tools::cicerone::TremplinStartedSignal& signal);
  void NotifyLxdContainerStarting(
      const vm_tools::cicerone::LxdContainerStartingSignal& signal);
  void NotifyExportLxdContainerProgress(
      const vm_tools::cicerone::ExportLxdContainerProgressSignal& signal);
  void NotifyImportLxdContainerProgress(
      const vm_tools::cicerone::ImportLxdContainerProgressSignal& signal);
  void InstallLinuxPackageProgress(
      const vm_tools::cicerone::InstallLinuxPackageProgressSignal& signal);
  void UninstallPackageProgress(
      const vm_tools::cicerone::UninstallPackageProgressSignal& signal);
  void NotifyPendingAppListUpdates(
      const vm_tools::cicerone::PendingAppListUpdatesSignal& signal);
  void NotifyApplyAnsiblePlaybookProgress(
      const vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal& signal);
  void NotifyUpgradeContainerProgress(
      const vm_tools::cicerone::UpgradeContainerProgressSignal& signal);

 protected:
  void Init(dbus::Bus* bus) override {}

 private:
  bool is_container_started_signal_connected_ = true;
  bool is_container_shutdown_signal_connected_ = true;
  bool is_install_linux_package_progress_signal_connected_ = true;
  bool is_uninstall_package_progress_signal_connected_ = true;
  bool is_lxd_container_created_signal_connected_ = true;
  bool is_lxd_container_deleted_signal_connected_ = true;
  bool is_lxd_container_downloading_signal_connected_ = true;
  bool is_tremplin_started_signal_connected_ = true;
  bool is_lxd_container_starting_signal_connected_ = true;
  bool is_export_lxd_container_progress_signal_connected_ = true;
  bool is_import_lxd_container_progress_signal_connected_ = true;
  bool is_apply_ansible_playbook_progress_signal_connected_ = true;
  bool is_upgrade_container_progress_signal_connected_ = true;

  std::string last_container_username_;
  bool send_container_started_signal_ = true;

  vm_tools::cicerone::LxdContainerCreatedSignal_Status
      lxd_container_created_signal_status_ =
          vm_tools::cicerone::LxdContainerCreatedSignal::CREATED;
  vm_tools::cicerone::LxdContainerDeletedSignal_Status
      lxd_container_deleted_signal_status_ =
          vm_tools::cicerone::LxdContainerDeletedSignal::DELETED;
  vm_tools::cicerone::LxdContainerStartingSignal_Status
      lxd_container_starting_signal_status_ =
          vm_tools::cicerone::LxdContainerStartingSignal::STARTED;

  vm_tools::cicerone::LaunchContainerApplicationResponse
      launch_container_application_response_;
  vm_tools::cicerone::ContainerAppIconResponse container_app_icon_response_;
  vm_tools::cicerone::LinuxPackageInfoRequest
      most_recent_linux_package_info_request_;
  vm_tools::cicerone::LinuxPackageInfoResponse get_linux_package_info_response_;
  vm_tools::cicerone::InstallLinuxPackageRequest
      most_recent_install_linux_package_request_;
  vm_tools::cicerone::InstallLinuxPackageResponse
      install_linux_package_response_;
  vm_tools::cicerone::UninstallPackageOwningFileResponse
      uninstall_package_owning_file_response_;
  vm_tools::cicerone::CreateLxdContainerResponse create_lxd_container_response_;
  vm_tools::cicerone::DeleteLxdContainerResponse delete_lxd_container_response_;
  vm_tools::cicerone::StartLxdContainerResponse start_lxd_container_response_;
  vm_tools::cicerone::GetLxdContainerUsernameResponse
      get_lxd_container_username_response_;
  vm_tools::cicerone::SetUpLxdContainerUserResponse
      setup_lxd_container_user_response_;
  vm_tools::cicerone::ExportLxdContainerResponse export_lxd_container_response_;
  vm_tools::cicerone::ImportLxdContainerResponse import_lxd_container_response_;
  vm_tools::cicerone::CancelExportLxdContainerResponse
      cancel_export_lxd_container_response_;
  vm_tools::cicerone::CancelImportLxdContainerResponse
      cancel_import_lxd_container_response_;
  vm_tools::cicerone::ApplyAnsiblePlaybookResponse
      apply_ansible_playbook_response_;
  vm_tools::cicerone::UpgradeContainerResponse upgrade_container_response_;
  vm_tools::cicerone::CancelUpgradeContainerResponse
      cancel_upgrade_container_response_;

  vm_tools::cicerone::OsRelease lxd_container_os_release_;

  UninstallPackageOwningFileCallback uninstall_package_owning_file_callback_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(FakeCiceroneClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_CICERONE_CLIENT_H_
