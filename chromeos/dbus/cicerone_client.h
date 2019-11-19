// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CICERONE_CLIENT_H_
#define CHROMEOS_DBUS_CICERONE_CLIENT_H_

#include <memory>

#include "base/component_export.h"
#include "chromeos/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// CiceroneClient is used to communicate with Cicerone, which is used to
// communicate with containers running inside VMs.
class COMPONENT_EXPORT(CHROMEOS_DBUS) CiceroneClient : public DBusClient {
 public:
  class Observer {
   public:
    // OnContainerStarted is signaled by Cicerone after the long-running Lxd
    // container startup process has been completed and the container is ready.
    virtual void OnContainerStarted(
        const vm_tools::cicerone::ContainerStartedSignal& signal) = 0;

    // OnContainerShutdown is signaled by Cicerone when a container is shutdown.
    virtual void OnContainerShutdown(
        const vm_tools::cicerone::ContainerShutdownSignal& signal) = 0;

    // This is signaled from the container while a package is being installed
    // via InstallLinuxPackage.
    virtual void OnInstallLinuxPackageProgress(
        const vm_tools::cicerone::InstallLinuxPackageProgressSignal&
            signal) = 0;

    // This is signaled from the container while a package is being uninstalled
    // via UninstallPackageOwningFile.
    virtual void OnUninstallPackageProgress(
        const vm_tools::cicerone::UninstallPackageProgressSignal& signal) = 0;

    // OnLxdContainerCreated is signaled from Cicerone when the long running
    // creation of an Lxd container is complete.
    virtual void OnLxdContainerCreated(
        const vm_tools::cicerone::LxdContainerCreatedSignal& signal) = 0;

    // OnLxdContainerDeleted is signaled from Cicerone when the long running
    // deletion of an Lxd container is complete.
    virtual void OnLxdContainerDeleted(
        const vm_tools::cicerone::LxdContainerDeletedSignal& signal) = 0;

    // OnLxdContainerDownloading is signaled from Cicerone giving download
    // progress on the container.
    virtual void OnLxdContainerDownloading(
        const vm_tools::cicerone::LxdContainerDownloadingSignal& signal) = 0;

    // OnTremplinStarted is signaled from Cicerone when Tremplin gRPC service is
    // first connected in a VM. This service is required for CreateLxdContainer
    // and StartLxdContainer.
    virtual void OnTremplinStarted(
        const vm_tools::cicerone::TremplinStartedSignal& signal) = 0;

    // OnLxdContainerStarting is signaled from Cicerone when async container
    // startup is used. This is necessary if long running file remapping is
    // required before an old container is safe to use.
    virtual void OnLxdContainerStarting(
        const vm_tools::cicerone::LxdContainerStartingSignal& signal) = 0;

    // OnExportLxdContainerProgress is signalled from Cicerone while a container
    // is being exported via ExportLxdContainer.
    virtual void OnExportLxdContainerProgress(
        const vm_tools::cicerone::ExportLxdContainerProgressSignal& signal) = 0;

    // OnImportLxdContainerProgress is signalled from Cicerone while a container
    // is being imported via ImportLxdContainer.
    virtual void OnImportLxdContainerProgress(
        const vm_tools::cicerone::ImportLxdContainerProgressSignal& signal) = 0;

    // OnPendingAppListUpdates is signalled from Cicerone when the number of
    // pending app list updates changes.
    virtual void OnPendingAppListUpdates(
        const vm_tools::cicerone::PendingAppListUpdatesSignal& signal) = 0;

    // This is signaled from the container while a playbook is being applied
    // via ApplyAnsiblePlaybook.
    virtual void OnApplyAnsiblePlaybookProgress(
        const vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal&
            signal) = 0;

    // This is signaled from Cicerone while a container is being upgraded
    // via UpgradeContainer.
    virtual void OnUpgradeContainerProgress(
        const vm_tools::cicerone::UpgradeContainerProgressSignal& signal) = 0;

   protected:
    virtual ~Observer() = default;
  };

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

  // This should be true before expecting to recieve
  // PendingAppListUpdatesSignal.
  virtual bool IsPendingAppListUpdatesSignalConnected() = 0;

  // This should be true prior to calling ApplyAnsiblePlaybook.
  virtual bool IsApplyAnsiblePlaybookProgressSignalConnected() = 0;

  // This should be true prior to calling UpgradeContainer.
  virtual bool IsUpgradeContainerProgressSignalConnected() = 0;

  // Launches an application inside a running Container.
  // |callback| is called after the method call finishes.
  virtual void LaunchContainerApplication(
      const vm_tools::cicerone::LaunchContainerApplicationRequest& request,
      DBusMethodCallback<vm_tools::cicerone::LaunchContainerApplicationResponse>
          callback) = 0;

  // Gets application icons from inside a Container.
  // |callback| is called after the method call finishes.
  virtual void GetContainerAppIcons(
      const vm_tools::cicerone::ContainerAppIconRequest& request,
      DBusMethodCallback<vm_tools::cicerone::ContainerAppIconResponse>
          callback) = 0;

  // Gets information about a Linux package file inside a container.
  // |callback| is called after the method call finishes.
  virtual void GetLinuxPackageInfo(
      const vm_tools::cicerone::LinuxPackageInfoRequest& request,
      DBusMethodCallback<vm_tools::cicerone::LinuxPackageInfoResponse>
          callback) = 0;

  // Installs a package inside the container.
  // |callback| is called after the method call finishes.
  virtual void InstallLinuxPackage(
      const vm_tools::cicerone::InstallLinuxPackageRequest& request,
      DBusMethodCallback<vm_tools::cicerone::InstallLinuxPackageResponse>
          callback) = 0;

  // Uninstalls the package that owns the indicated .desktop file.
  // |callback| is called after the method call finishes.
  virtual void UninstallPackageOwningFile(
      const vm_tools::cicerone::UninstallPackageOwningFileRequest& request,
      DBusMethodCallback<vm_tools::cicerone::UninstallPackageOwningFileResponse>
          callback) = 0;

  // Creates a new Lxd Container.
  // |callback| is called to indicate creation status.
  // |Observer::OnLxdContainerCreated| will be called on completion.
  // |Observer::OnLxdContainerDownloading| is called to indicate progress.
  virtual void CreateLxdContainer(
      const vm_tools::cicerone::CreateLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::CreateLxdContainerResponse>
          callback) = 0;

  // Deletes an Lxd Container.
  // |callback| is called to indicate deletion status.
  // |Observer::OnLxdContainerDeleted| will be called on completion.
  virtual void DeleteLxdContainer(
      const vm_tools::cicerone::DeleteLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::DeleteLxdContainerResponse>
          callback) = 0;

  // Starts a new Lxd Container.
  // |callback| is called when the method completes.
  virtual void StartLxdContainer(
      const vm_tools::cicerone::StartLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::StartLxdContainerResponse>
          callback) = 0;

  // Gets the Lxd container username.
  // |callback| is called when the method completes.
  virtual void GetLxdContainerUsername(
      const vm_tools::cicerone::GetLxdContainerUsernameRequest& request,
      DBusMethodCallback<vm_tools::cicerone::GetLxdContainerUsernameResponse>
          callback) = 0;

  // Sets the Lxd container user, creating it if needed.
  // |callback| is called when the method completes.
  virtual void SetUpLxdContainerUser(
      const vm_tools::cicerone::SetUpLxdContainerUserRequest& request,
      DBusMethodCallback<vm_tools::cicerone::SetUpLxdContainerUserResponse>
          callback) = 0;

  // Exports the Lxd container.
  // |callback| is called when the method completes.
  virtual void ExportLxdContainer(
      const vm_tools::cicerone::ExportLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::ExportLxdContainerResponse>
          callback) = 0;

  // Imports the Lxd container.
  // |callback| is called when the method completes.
  virtual void ImportLxdContainer(
      const vm_tools::cicerone::ImportLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::ImportLxdContainerResponse>
          callback) = 0;

  // Cancels the in progress Lxd container export.
  // |callback| is called when the method completes.
  virtual void CancelExportLxdContainer(
      const vm_tools::cicerone::CancelExportLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::CancelExportLxdContainerResponse>
          callback) = 0;

  // Cancels the in progress Lxd container import.
  // |callback| is called when the method completes.
  virtual void CancelImportLxdContainer(
      const vm_tools::cicerone::CancelImportLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::CancelImportLxdContainerResponse>
          callback) = 0;

  // Applies Ansible playbook.
  // |callback| is called after the method call finishes.
  virtual void ApplyAnsiblePlaybook(
      const vm_tools::cicerone::ApplyAnsiblePlaybookRequest& request,
      DBusMethodCallback<vm_tools::cicerone::ApplyAnsiblePlaybookResponse>
          callback) = 0;

  // Upgrades the container.
  // |callback| is called when the method completes.
  virtual void UpgradeContainer(
      const vm_tools::cicerone::UpgradeContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::UpgradeContainerResponse>
          callback) = 0;

  // Cancels the in progress container upgrade.
  // |callback| is called when the method completes.
  virtual void CancelUpgradeContainer(
      const vm_tools::cicerone::CancelUpgradeContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::CancelUpgradeContainerResponse>
          callback) = 0;

  // Registers |callback| to run when the Cicerone service becomes available.
  // If the service is already available, or if connecting to the name-owner-
  // changed signal fails, |callback| will be run once asynchronously.
  // Otherwise, |callback| will be run once in the future after the service
  // becomes available.
  virtual void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) = 0;

  // Creates an instance of CiceroneClient.
  static std::unique_ptr<CiceroneClient> Create();

 protected:
  // Create() should be used instead.
  CiceroneClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(CiceroneClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CICERONE_CLIENT_H_
