// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CICERONE_CLIENT_H_
#define CHROMEOS_DBUS_CICERONE_CLIENT_H_

#include <memory>

#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// CiceroneClient is used to communicate with Cicerone, which is used to
// communicate with containers running inside VMs.
class CHROMEOS_EXPORT CiceroneClient : public DBusClient {
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

    // OnLxdContainerCreated is signaled from Cicerone when the long running
    // creation of an Lxd container is complete.
    virtual void OnLxdContainerCreated(
        const vm_tools::cicerone::LxdContainerCreatedSignal& signal) = 0;

    // OnLxdContainerDownloading is signaled from Cicerone giving download
    // progress on the container.
    virtual void OnLxdContainerDownloading(
        const vm_tools::cicerone::LxdContainerDownloadingSignal& signal) = 0;

    // OnTremplinStarted is signaled from Cicerone when Tremplin gRPC service is
    // first connected in a VM. This service is required for CreateLxdContainer
    // and StartLxdContainer.
    virtual void OnTremplinStarted(
        const vm_tools::cicerone::TremplinStartedSignal& signal) = 0;

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

  // This should be true prior to calling CreateLxdContainer or
  // StartLxdContainer.
  virtual bool IsLxdContainerCreatedSignalConnected() = 0;

  // This should be true prior to calling CreateLxdContainer or
  // StartLxdContainer.
  virtual bool IsLxdContainerDownloadingSignalConnected() = 0;

  // This should be true prior to calling CreateLxdContainer or
  // StartLxdContainer.
  virtual bool IsTremplinStartedSignalConnected() = 0;

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

  // Creates a new Lxd Container.
  // |callback| is called to indicate creation status.
  // |Observer::OnLxdContainerCreated| will be called on completion.
  // |Observer::OnLxdContainerDownloading| is called to indicate progress.
  virtual void CreateLxdContainer(
      const vm_tools::cicerone::CreateLxdContainerRequest& request,
      DBusMethodCallback<vm_tools::cicerone::CreateLxdContainerResponse>
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
