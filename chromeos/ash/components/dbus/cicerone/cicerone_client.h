// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CICERONE_CICERONE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CICERONE_CICERONE_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "dbus/object_proxy.h"

namespace ash {

// CiceroneClient is used to communicate with Cicerone, which is used to
// communicate with containers running inside VMs.
class COMPONENT_EXPORT(CICERONE) CiceroneClient : public chromeos::DBusClient {
 public:
  class Observer {
   public:
    // Called when Cicerone service exits.
    virtual void CiceroneServiceStopped() {}

    // Called when Cicerone service is either started or restarted.
    virtual void CiceroneServiceStarted() {}

    // OnContainerStarted is signaled by Cicerone after the long-running Lxd
    // container startup process has been completed and the container is ready.
    virtual void OnContainerStarted(
        const vm_tools::cicerone::ContainerStartedSignal& signal) {}

    // OnContainerShutdown is signaled by Cicerone when a container is shutdown.
    virtual void OnContainerShutdown(
        const vm_tools::cicerone::ContainerShutdownSignal& signal) {}

    // This is signaled from the container while a package is being installed
    // via InstallLinuxPackage.
    virtual void OnInstallLinuxPackageProgress(
        const vm_tools::cicerone::InstallLinuxPackageProgressSignal& signal) {}

    // This is signaled from the container while a package is being uninstalled
    // via UninstallPackageOwningFile.
    virtual void OnUninstallPackageProgress(
        const vm_tools::cicerone::UninstallPackageProgressSignal& signal) {}

    // OnLxdContainerCreated is signaled from Cicerone when the long running
    // creation of an Lxd container is complete.
    virtual void OnLxdContainerCreated(
        const vm_tools::cicerone::LxdContainerCreatedSignal& signal) {}

    // OnLxdContainerDeleted is signaled from Cicerone when the long running
    // deletion of an Lxd container is complete.
    virtual void OnLxdContainerDeleted(
        const vm_tools::cicerone::LxdContainerDeletedSignal& signal) {}

    // OnLxdContainerDownloading is signaled from Cicerone giving download
    // progress on the container.
    virtual void OnLxdContainerDownloading(
        const vm_tools::cicerone::LxdContainerDownloadingSignal& signal) {}

    // OnTremplinStarted is signaled from Cicerone when Tremplin gRPC service is
    // first connected in a VM. This service is required for CreateLxdContainer
    // and StartLxdContainer.
    virtual void OnTremplinStarted(
        const vm_tools::cicerone::TremplinStartedSignal& signal) {}

    // OnLxdContainerStarting is signaled from Cicerone when async container
    // startup is used. This is necessary if long running file remapping is
    // required before an old container is safe to use.
    virtual void OnLxdContainerStarting(
        const vm_tools::cicerone::LxdContainerStartingSignal& signal) {}

    // OnExportLxdContainerProgress is signalled from Cicerone while a container
    // is being exported via ExportLxdContainer.
    virtual void OnExportLxdContainerProgress(
        const vm_tools::cicerone::ExportLxdContainerProgressSignal& signal) {}

    // OnImportLxdContainerProgress is signalled from Cicerone while a container
    // is being imported via ImportLxdContainer.
    virtual void OnImportLxdContainerProgress(
        const vm_tools::cicerone::ImportLxdContainerProgressSignal& signal) {}

    // OnPendingAppListUpdates is signalled from Cicerone when the number of
    // pending app list updates changes.
    virtual void OnPendingAppListUpdates(
        const vm_tools::cicerone::PendingAppListUpdatesSignal& signal) {}

    // This is signaled from the container while a playbook is being applied
    // via ApplyAnsiblePlaybook.
    virtual void OnApplyAnsiblePlaybookProgress(
        const vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal& signal) {}

    // This is signaled from Cicerone while a container is being upgraded
    // via UpgradeContainer.
    virtual void OnUpgradeContainerProgress(
        const vm_tools::cicerone::UpgradeContainerProgressSignal& signal) {}

    // This is signaled from Cicerone while LXD is starting via StartLxd.
    virtual void OnStartLxdProgress(
        const vm_tools::cicerone::StartLxdProgressSignal& signal) {}

    // This is signaled from Cicerone when a file in a watched directory is
    // changed.  It is used by FilesApp.
    virtual void OnFileWatchTriggered(
        const vm_tools::cicerone::FileWatchTriggeredSignal& signal) {}

    // This is signaled from Cicerone when a container runs into low disk space.
    virtual void OnLowDiskSpaceTriggered(
        const vm_tools::cicerone::LowDiskSpaceTriggeredSignal& signal) {}

    // This is signaled from Cicerone when the VM is requesting to inhibit
    // sleep.
    virtual void OnInhibitScreensaver(
        const vm_tools::cicerone::InhibitScreensaverSignal& signal) {}

    // This is signaled from Cicerone when the VM is requesting to uninhibit
    // sleep.
    virtual void OnUninhibitScreensaver(
        const vm_tools::cicerone::UninhibitScreensaverSignal& signal) {}

   protected:
    virtual ~Observer() = default;
  };

  CiceroneClient(const CiceroneClient&) = delete;
  CiceroneClient& operator=(const CiceroneClient&) = delete;

  ~CiceroneClient() override;

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer if added.
  virtual void RemoveObserver(Observer* observer) = 0;

  // IsContainerStartedSignalConnected must return true before StartLxdContainer
  // is called.
  virtual bool IsContainerStartedSignalConnected() = 0;

  // IsContainerShutdownSignalConnected must return true before
  // StartLxdContainer is called.
  virtual bool IsContainerShutdownSignalConnected() = 0;

  // This should be true prior to calling InstallLinuxPackage.
  virtual bool IsInstallLinuxPackageProgressSignalConnected() = 0;

  // This should be true prior to calling UninstallPackageOwningFile.
  virtual bool IsUninstallPackageProgressSignalConnected() = 0;

  // This should be true prior to calling CreateLxdContainer or
  // StartLxdContainer.
  virtual bool IsLxdContainerCreatedSignalConnected() = 0;

  // This should be true prior to calling DeleteLxdContainer.
  virtual bool IsLxdContainerDeletedSignalConnected() = 0;

  // This should be true prior to calling CreateLxdContainer or
  // StartLxdContainer.
  virtual bool IsLxdContainerDownloadingSignalConnected() = 0;

  // This should be true prior to calling CreateLxdContainer or
  // StartLxdContainer.
  virtual bool IsTremplinStartedSignalConnected() = 0;

  // This should be true prior to calling StartLxdContainer in async mode.
  virtual bool IsLxdContainerStartingSignalConnected() = 0;

  // This should be true prior to calling ExportLxdContainer.
  virtual bool IsExportLxdContainerProgressSignalConnected() = 0;

  // This should be true prior to calling ImportLxdContainer.
  virtual bool IsImportLxdContainerProgressSignalConnected() = 0;

  // This should be true before expecting to receive
  // PendingAppListUpdatesSignal.
  virtual bool IsPendingAppListUpdatesSignalConnected() = 0;

  // This should be true prior to calling ApplyAnsiblePlaybook.
  virtual bool IsApplyAnsiblePlaybookProgressSignalConnected() = 0;

  // This should be true prior to calling UpgradeContainer.
  virtual bool IsUpgradeContainerProgressSignalConnected() = 0;

  // This should be true prior to calling StartLxd.
  virtual bool IsStartLxdProgressSignalConnected() = 0;

  // This should be true prior to calling AddFileWatch.
  virtual bool IsFileWatchTriggeredSignalConnected() = 0;

  // This should be true before expecting to receive
  // LowDiskSpaceTriggeredSignal.
  virtual bool IsLowDiskSpaceTriggeredSignalConnected() = 0;

  // This should be true before expecting to receive
  // InhibitScreensaverSignal
  virtual bool IsInhibitScreensaverSignalConencted() = 0;

  // This should be true before expecting to receive
  // UninhibitScreensaverSignal.
  virtual bool IsUninhibitScreensaverSignalConencted() = 0;

  // Launches an application inside a running Container.
  // |callback| is called after the method call finishes.
  virtual void LaunchContainerApplication(
      const vm_tools::cicerone::LaunchContainerApplicationRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::LaunchContainerApplicationResponse> callback) = 0;

  // Gets application icons from inside a Container.
  // |callback| is called after the method call finishes.
  virtual void GetContainerAppIcons(
      const vm_tools::cicerone::ContainerAppIconRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::ContainerAppIconResponse>
          callback) = 0;

  // Gets information about a Linux package file inside a container.
  // |callback| is called after the method call finishes.
  virtual void GetLinuxPackageInfo(
      const vm_tools::cicerone::LinuxPackageInfoRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::LinuxPackageInfoResponse>
          callback) = 0;

  // Installs a package inside the container.
  // |callback| is called after the method call finishes.
  virtual void InstallLinuxPackage(
      const vm_tools::cicerone::InstallLinuxPackageRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::InstallLinuxPackageResponse> callback) = 0;

  // Uninstalls the package that owns the indicated .desktop file.
  // |callback| is called after the method call finishes.
  virtual void UninstallPackageOwningFile(
      const vm_tools::cicerone::UninstallPackageOwningFileRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::UninstallPackageOwningFileResponse> callback) = 0;

  // Creates a new Lxd Container.
  // |callback| is called to indicate creation status.
  // |Observer::OnLxdContainerCreated| will be called on completion.
  // |Observer::OnLxdContainerDownloading| is called to indicate progress.
  virtual void CreateLxdContainer(
      const vm_tools::cicerone::CreateLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::CreateLxdContainerResponse> callback) = 0;

  // Deletes an Lxd Container.
  // |callback| is called to indicate deletion status.
  // |Observer::OnLxdContainerDeleted| will be called on completion.
  virtual void DeleteLxdContainer(
      const vm_tools::cicerone::DeleteLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::DeleteLxdContainerResponse> callback) = 0;

  // Starts a new Lxd Container.
  // |callback| is called when the method completes.
  virtual void StartLxdContainer(
      const vm_tools::cicerone::StartLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::StartLxdContainerResponse> callback) = 0;

  // Stops a running Lxd Container.
  // |callback| is called when the method completes.
  virtual void StopLxdContainer(
      const vm_tools::cicerone::StopLxdContainerRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::StopLxdContainerResponse>
          callback) = 0;

  // Gets the Lxd container username.
  // |callback| is called when the method completes.
  virtual void GetLxdContainerUsername(
      const vm_tools::cicerone::GetLxdContainerUsernameRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::GetLxdContainerUsernameResponse> callback) = 0;

  // Sets the Lxd container user, creating it if needed.
  // |callback| is called when the method completes.
  virtual void SetUpLxdContainerUser(
      const vm_tools::cicerone::SetUpLxdContainerUserRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::SetUpLxdContainerUserResponse> callback) = 0;

  // Exports the Lxd container.
  // |callback| is called when the method completes.
  virtual void ExportLxdContainer(
      const vm_tools::cicerone::ExportLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ExportLxdContainerResponse> callback) = 0;

  // Imports the Lxd container.
  // |callback| is called when the method completes.
  virtual void ImportLxdContainer(
      const vm_tools::cicerone::ImportLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ImportLxdContainerResponse> callback) = 0;

  // Cancels the in progress Lxd container export.
  // |callback| is called when the method completes.
  virtual void CancelExportLxdContainer(
      const vm_tools::cicerone::CancelExportLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::CancelExportLxdContainerResponse> callback) = 0;

  // Cancels the in progress Lxd container import.
  // |callback| is called when the method completes.
  virtual void CancelImportLxdContainer(
      const vm_tools::cicerone::CancelImportLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::CancelImportLxdContainerResponse> callback) = 0;

  // Applies Ansible playbook.
  // |callback| is called after the method call finishes.
  virtual void ApplyAnsiblePlaybook(
      const vm_tools::cicerone::ApplyAnsiblePlaybookRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ApplyAnsiblePlaybookResponse> callback) = 0;

  // Configure the container to allow sideloading Android apps into Arc.
  // |callback| is called once configuration finishes.
  virtual void ConfigureForArcSideload(
      const vm_tools::cicerone::ConfigureForArcSideloadRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ConfigureForArcSideloadResponse> callback) = 0;

  // Upgrades the container.
  // |callback| is called when the method completes.
  virtual void UpgradeContainer(
      const vm_tools::cicerone::UpgradeContainerRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::UpgradeContainerResponse>
          callback) = 0;

  // Cancels the in progress container upgrade.
  // |callback| is called when the method completes.
  virtual void CancelUpgradeContainer(
      const vm_tools::cicerone::CancelUpgradeContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::CancelUpgradeContainerResponse> callback) = 0;

  // Starts LXD.
  // |callback| is called when the method completes.
  virtual void StartLxd(
      const vm_tools::cicerone::StartLxdRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::StartLxdResponse>
          callback) = 0;

  // Adds a file watcher.  Used by FilesApp.
  // |callback| is called when the method completes.
  virtual void AddFileWatch(
      const vm_tools::cicerone::AddFileWatchRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::AddFileWatchResponse>
          callback) = 0;

  // Removes a file watch.
  // |callback| is called when the method completes.
  virtual void RemoveFileWatch(
      const vm_tools::cicerone::RemoveFileWatchRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::RemoveFileWatchResponse>
          callback) = 0;

  // Looks up vsh session data such as container shell pid.
  // |callback| is called when the method completes.
  virtual void GetVshSession(
      const vm_tools::cicerone::GetVshSessionRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::GetVshSessionResponse>
          callback) = 0;

  // Attaches a USB device to a LXD container.
  // |callback| is called when the method completes.
  virtual void AttachUsbToContainer(
      const vm_tools::cicerone::AttachUsbToContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::AttachUsbToContainerResponse> callback) = 0;

  // Detaches a USB device from a LXD container.
  // |callback| is called when the method completes.
  virtual void DetachUsbFromContainer(
      const vm_tools::cicerone::DetachUsbFromContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::DetachUsbFromContainerResponse> callback) = 0;

  // Send signal with files user has selected in SelectFile dialog. This is sent
  // in response to VmApplicationsServiceProvider::SelectFile().
  virtual void FileSelected(
      const vm_tools::cicerone::FileSelectedSignal& signal) = 0;

  // Lists the containers Cicerone knows about.
  // |callback| is called when the method completes.
  virtual void ListRunningContainers(
      const vm_tools::cicerone::ListRunningContainersRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ListRunningContainersResponse> callback) = 0;

  // Queries Garcon for info about the current session.
  // |callback| is called when the method completes.
  virtual void GetGarconSessionInfo(
      const vm_tools::cicerone::GetGarconSessionInfoRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::GetGarconSessionInfoResponse> callback) = 0;

  // Updates the VM devices available for a LXD container.
  // |callback| is called when the method completes.
  virtual void UpdateContainerDevices(
      const vm_tools::cicerone::UpdateContainerDevicesRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::UpdateContainerDevicesResponse> callback) = 0;

  // Registers |callback| to run when the Cicerone service becomes available.
  // If the service is already available, or if connecting to the name-owner-
  // changed signal fails, |callback| will be run once asynchronously.
  // Otherwise, |callback| will be run once in the future after the service
  // becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static CiceroneClient* Get();

 protected:
  // Initialize() should be used instead.
  CiceroneClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CICERONE_CICERONE_CLIENT_H_
