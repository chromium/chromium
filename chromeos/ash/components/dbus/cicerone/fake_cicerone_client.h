// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CICERONE_FAKE_CICERONE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CICERONE_FAKE_CICERONE_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"

namespace ash {

// FakeCiceroneClient is a fake implementation of CiceroneClient used for
// testing.
class COMPONENT_EXPORT(CICERONE) FakeCiceroneClient : public CiceroneClient {
 public:
  using LaunchContainerApplicationCallback = base::RepeatingCallback<void(
      const vm_tools::cicerone::LaunchContainerApplicationRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::LaunchContainerApplicationResponse> callback)>;
  using UninstallPackageOwningFileCallback = base::RepeatingCallback<void(
      const vm_tools::cicerone::UninstallPackageOwningFileRequest&,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::UninstallPackageOwningFileResponse>)>;

  // Returns the fake global instance if initialized. May return null.
  static FakeCiceroneClient* Get();

  FakeCiceroneClient(const FakeCiceroneClient&) = delete;
  FakeCiceroneClient& operator=(const FakeCiceroneClient&) = delete;

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
  bool IsStartLxdProgressSignalConnected() override;
  bool IsFileWatchTriggeredSignalConnected() override;
  bool IsLowDiskSpaceTriggeredSignalConnected() override;
  bool IsInhibitScreensaverSignalConencted() override;
  bool IsUninhibitScreensaverSignalConencted() override;
  void LaunchContainerApplication(
      const vm_tools::cicerone::LaunchContainerApplicationRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::LaunchContainerApplicationResponse> callback)
      override;
  void GetContainerAppIcons(
      const vm_tools::cicerone::ContainerAppIconRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::ContainerAppIconResponse>
          callback) override;
  void GetLinuxPackageInfo(
      const vm_tools::cicerone::LinuxPackageInfoRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::LinuxPackageInfoResponse>
          callback) override;
  // Fake version of the method that installs an application inside a running
  // Container. |callback| is called after the method call finishes. This does
  // not cause progress events to be fired.
  void InstallLinuxPackage(
      const vm_tools::cicerone::InstallLinuxPackageRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::InstallLinuxPackageResponse> callback) override;
  // If SetOnUninstallPackageOwningFileCallback has been called, it
  // just triggers that callback. Otherwise, it generates a task to call
  // |callback| with the response from
  // set_uninstall_package_owning_file_response.
  void UninstallPackageOwningFile(
      const vm_tools::cicerone::UninstallPackageOwningFileRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::UninstallPackageOwningFileResponse> callback)
      override;
  void CreateLxdContainer(
      const vm_tools::cicerone::CreateLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::CreateLxdContainerResponse> callback) override;
  void DeleteLxdContainer(
      const vm_tools::cicerone::DeleteLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::DeleteLxdContainerResponse> callback) override;
  void StartLxdContainer(
      const vm_tools::cicerone::StartLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::StartLxdContainerResponse> callback) override;
  void StopLxdContainer(
      const vm_tools::cicerone::StopLxdContainerRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::StopLxdContainerResponse>
          callback) override;
  void GetLxdContainerUsername(
      const vm_tools::cicerone::GetLxdContainerUsernameRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::GetLxdContainerUsernameResponse> callback)
      override;
  void SetUpLxdContainerUser(
      const vm_tools::cicerone::SetUpLxdContainerUserRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::SetUpLxdContainerUserResponse> callback) override;
  void ExportLxdContainer(
      const vm_tools::cicerone::ExportLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ExportLxdContainerResponse> callback) override;
  void ImportLxdContainer(
      const vm_tools::cicerone::ImportLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ImportLxdContainerResponse> callback) override;
  void CancelExportLxdContainer(
      const vm_tools::cicerone::CancelExportLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::CancelExportLxdContainerResponse> callback)
      override;
  void CancelImportLxdContainer(
      const vm_tools::cicerone::CancelImportLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::CancelImportLxdContainerResponse> callback)
      override;
  void ApplyAnsiblePlaybook(
      const vm_tools::cicerone::ApplyAnsiblePlaybookRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ApplyAnsiblePlaybookResponse> callback) override;
  void ConfigureForArcSideload(
      const vm_tools::cicerone::ConfigureForArcSideloadRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ConfigureForArcSideloadResponse> callback)
      override;
  void UpgradeContainer(
      const vm_tools::cicerone::UpgradeContainerRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::UpgradeContainerResponse>
          callback) override;
  void CancelUpgradeContainer(
      const vm_tools::cicerone::CancelUpgradeContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::CancelUpgradeContainerResponse> callback)
      override;
  void StartLxd(
      const vm_tools::cicerone::StartLxdRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::StartLxdResponse>
          callback) override;
  void AddFileWatch(
      const vm_tools::cicerone::AddFileWatchRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::AddFileWatchResponse>
          callback) override;
  void RemoveFileWatch(
      const vm_tools::cicerone::RemoveFileWatchRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::RemoveFileWatchResponse>
          callback) override;
  void GetVshSession(
      const vm_tools::cicerone::GetVshSessionRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::GetVshSessionResponse>
          callback) override;
  void AttachUsbToContainer(
      const vm_tools::cicerone::AttachUsbToContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::AttachUsbToContainerResponse> callback) override;
  void DetachUsbFromContainer(
      const vm_tools::cicerone::DetachUsbFromContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::DetachUsbFromContainerResponse> callback)
      override;
  void FileSelected(
      const vm_tools::cicerone::FileSelectedSignal& signal) override;
  void ListRunningContainers(
      const vm_tools::cicerone::ListRunningContainersRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ListRunningContainersResponse> callback) override;
  void GetGarconSessionInfo(
      const vm_tools::cicerone::GetGarconSessionInfoRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::GetGarconSessionInfoResponse> callback) override;
  void UpdateContainerDevices(
      const vm_tools::cicerone::UpdateContainerDevicesRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::UpdateContainerDevicesResponse> callback)
      override;
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

  // Sets a callback to be called during any call to LaunchContainerApplication.
  void SetOnLaunchContainerApplicationCallback(
      LaunchContainerApplicationCallback callback);
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
  void set_start_lxd_progress_signal_connected(bool connected) {
    is_start_lxd_progress_signal_connected_ = connected;
  }
  void set_file_watch_triggered_signal_connected(bool connected) {
    is_file_watch_triggered_signal_connected_ = connected;
  }
  void set_low_disk_space_triggered_signal_connected(bool connected) {
    is_low_disk_space_triggered_signal_connected_ = connected;
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
  void set_stop_lxd_container_response(
      const vm_tools::cicerone::StopLxdContainerResponse&
          stop_lxd_container_response) {
    stop_lxd_container_response_ = stop_lxd_container_response;
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
  void set_apply_ansible_playbook_response(
      const vm_tools::cicerone::ApplyAnsiblePlaybookResponse&
          apply_ansible_playbook_response) {
    apply_ansible_playbook_response_ = apply_ansible_playbook_response;
  }
  void set_enable_arc_sideload_response(
      const vm_tools::cicerone::ConfigureForArcSideloadResponse& response) {
    enable_arc_sideload_response_ = response;
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
  void set_start_lxd_response(
      vm_tools::cicerone::StartLxdResponse start_lxd_response) {
    start_lxd_response_ = std::move(start_lxd_response);
  }
  void set_add_file_watch_response(
      vm_tools::cicerone::AddFileWatchResponse add_file_watch_response) {
    add_file_watch_response_ = std::move(add_file_watch_response);
  }
  void set_remove_file_watch_response(
      vm_tools::cicerone::RemoveFileWatchResponse remove_file_watch_response) {
    remove_file_watch_response_ = std::move(remove_file_watch_response);
  }
  void set_get_vsh_session_response(
      vm_tools::cicerone::GetVshSessionResponse get_vsh_session_response) {
    get_vsh_session_response_ = std::move(get_vsh_session_response);
  }
  void set_attach_usb_to_container_response(
      vm_tools::cicerone::AttachUsbToContainerResponse
          attach_usb_to_container_response) {
    attach_usb_to_container_response_ =
        std::move(attach_usb_to_container_response);
  }
  void set_detach_usb_from_container_response(
      vm_tools::cicerone::DetachUsbFromContainerResponse
          detach_usb_from_container_response) {
    detach_usb_from_container_response_ =
        std::move(detach_usb_from_container_response);
  }
  void set_list_containers_response(
      vm_tools::cicerone::ListRunningContainersResponse
          list_container_response) {
    list_containers_response_ = std::move(list_container_response);
  }
  void set_get_garcon_session_info_response(
      vm_tools::cicerone::GetGarconSessionInfoResponse
          get_garcon_session_info_response) {
    get_garcon_session_info_response_ =
        std::move(get_garcon_session_info_response);
  }

  void set_update_container_devices_response(
      vm_tools::cicerone::UpdateContainerDevicesResponse
          update_container_devices_response) {
    update_container_devices_response_ =
        std::move(update_container_devices_response);
  }

  void set_send_container_starting_signal_delay(base::TimeDelta delay) {
    send_container_starting_signal_delay_ = delay;
  }
  void set_send_container_started_signal_delay(base::TimeDelta delay) {
    send_container_started_signal_delay_ = delay;
  }
  void set_send_start_lxd_response_delay(base::TimeDelta delay) {
    send_start_lxd_response_delay_ = delay;
  }
  void set_send_create_lxd_container_response_delay(base::TimeDelta delay) {
    send_create_lxd_container_response_delay_ = delay;
  }
  void set_send_notify_lxd_container_created_signal_delay(
      base::TimeDelta delay) {
    send_notify_lxd_container_created_signal_delay_ = delay;
  }
  void set_send_set_up_lxd_container_user_response_delay(
      base::TimeDelta delay) {
    send_set_up_lxd_container_user_response_delay_ = delay;
  }
  void set_send_start_lxd_container_response_delay(base::TimeDelta delay) {
    send_start_lxd_container_response_delay_ = delay;
  }
  void set_send_stop_lxd_container_response_delay(base::TimeDelta delay) {
    send_stop_lxd_container_response_delay_ = delay;
  }

  vm_tools::cicerone::SetUpLxdContainerUserRequest
  get_setup_lxd_container_user_request() {
    return setup_lxd_container_user_request_;
  }

  // Returns true if the method has been invoked at least once, false otherwise.
  bool configure_for_arc_sideload_called() {
    return configure_for_arc_sideload_called_;
  }

  int create_lxd_container_count() { return create_lxd_container_count_; }
  int start_lxd_container_count() { return start_lxd_container_count_; }
  int setup_lxd_container_user_count() {
    return setup_lxd_container_user_count_;
  }
  int start_lxd_count() { return start_lxd_count_; }
  int add_file_watch_call_count() { return add_file_watch_call_count_; }
  int remove_file_watch_call_count() { return remove_file_watch_call_count_; }

  // Additional functions to allow tests to trigger Signals.
  void NotifyLxdContainerCreated(
      const vm_tools::cicerone::LxdContainerCreatedSignal& signal);
  void NotifyLxdContainerDeleted(
      const vm_tools::cicerone::LxdContainerDeletedSignal& signal);
  void NotifyContainerStarted(
      const vm_tools::cicerone::ContainerStartedSignal& signal);
  void NotifyContainerShutdownSignal(
      const vm_tools::cicerone::ContainerShutdownSignal& signal);
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
  void NotifyStartLxdProgress(
      const vm_tools::cicerone::StartLxdProgressSignal& signal);
  void NotifyFileWatchTriggered(
      const vm_tools::cicerone::FileWatchTriggeredSignal& signal);

  void NotifyCiceroneStopped();
  void NotifyCiceroneStarted();

 protected:
  friend class CiceroneClient;

  FakeCiceroneClient();
  ~FakeCiceroneClient() override;

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
  bool is_start_lxd_progress_signal_connected_ = true;
  bool is_file_watch_triggered_signal_connected_ = true;
  bool is_low_disk_space_triggered_signal_connected_ = true;
  bool is_inhibit_screensaver_signal_connected_ = true;
  bool is_uninhibit_screensaver_signal_connected_ = true;

  std::string last_container_username_;

  bool configure_for_arc_sideload_called_ = false;

  int create_lxd_container_count_ = 0;
  int start_lxd_container_count_ = 0;
  int setup_lxd_container_user_count_ = 0;
  int start_lxd_count_ = 0;
  int add_file_watch_call_count_ = 0;
  int remove_file_watch_call_count_ = 0;

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
  vm_tools::cicerone::StopLxdContainerResponse stop_lxd_container_response_;
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
  vm_tools::cicerone::ConfigureForArcSideloadResponse
      enable_arc_sideload_response_;
  vm_tools::cicerone::UpgradeContainerResponse upgrade_container_response_;
  vm_tools::cicerone::CancelUpgradeContainerResponse
      cancel_upgrade_container_response_;
  vm_tools::cicerone::StartLxdResponse start_lxd_response_;
  vm_tools::cicerone::AddFileWatchResponse add_file_watch_response_;
  vm_tools::cicerone::RemoveFileWatchResponse remove_file_watch_response_;
  vm_tools::cicerone::GetVshSessionResponse get_vsh_session_response_;
  vm_tools::cicerone::AttachUsbToContainerResponse
      attach_usb_to_container_response_;
  vm_tools::cicerone::DetachUsbFromContainerResponse
      detach_usb_from_container_response_;
  vm_tools::cicerone::ListRunningContainersResponse list_containers_response_;
  vm_tools::cicerone::GetGarconSessionInfoResponse
      get_garcon_session_info_response_;
  vm_tools::cicerone::UpdateContainerDevicesResponse
      update_container_devices_response_;

  base::TimeDelta send_container_starting_signal_delay_;
  base::TimeDelta send_container_started_signal_delay_;
  base::TimeDelta send_start_lxd_response_delay_;
  base::TimeDelta send_create_lxd_container_response_delay_;
  base::TimeDelta send_notify_lxd_container_created_signal_delay_;
  base::TimeDelta send_set_up_lxd_container_user_response_delay_;
  base::TimeDelta send_start_lxd_container_response_delay_;
  base::TimeDelta send_stop_lxd_container_response_delay_;

  vm_tools::cicerone::SetUpLxdContainerUserRequest
      setup_lxd_container_user_request_;

  vm_tools::cicerone::OsRelease lxd_container_os_release_;

  LaunchContainerApplicationCallback launch_container_application_callback_;
  UninstallPackageOwningFileCallback uninstall_package_owning_file_callback_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  base::WeakPtrFactory<FakeCiceroneClient> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CICERONE_FAKE_CICERONE_CLIENT_H_
