// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/vm_cicerone/dbus-constants.h"

namespace ash {

namespace {

CiceroneClient* g_instance = nullptr;

// How long to wait before timing out on regular RPCs.
constexpr base::TimeDelta kDefaultTimeout = base::Minutes(1);

// How long to wait while doing more complex operations like starting or
// creating a container.
constexpr base::TimeDelta kLongOperationTimeout = base::Minutes(3);

}  // namespace

class CiceroneClientImpl : public CiceroneClient {
 public:
  CiceroneClientImpl() = default;

  CiceroneClientImpl(const CiceroneClientImpl&) = delete;
  CiceroneClientImpl& operator=(const CiceroneClientImpl&) = delete;

  ~CiceroneClientImpl() override = default;

  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  bool IsContainerStartedSignalConnected() override {
    return is_container_started_signal_connected_;
  }

  bool IsContainerShutdownSignalConnected() override {
    return is_container_shutdown_signal_connected_;
  }

  bool IsInstallLinuxPackageProgressSignalConnected() override {
    return is_install_linux_package_progress_signal_connected_;
  }

  bool IsUninstallPackageProgressSignalConnected() override {
    return is_uninstall_package_progress_signal_connected_;
  }

  bool IsLxdContainerCreatedSignalConnected() override {
    return is_lxd_container_created_signal_connected_;
  }

  bool IsLxdContainerDeletedSignalConnected() override {
    return is_lxd_container_deleted_signal_connected_;
  }

  bool IsLxdContainerDownloadingSignalConnected() override {
    return is_lxd_container_downloading_signal_connected_;
  }

  bool IsTremplinStartedSignalConnected() override {
    return is_tremplin_started_signal_connected_;
  }

  bool IsLxdContainerStartingSignalConnected() override {
    return is_lxd_container_starting_signal_connected_;
  }

  bool IsExportLxdContainerProgressSignalConnected() override {
    return is_export_lxd_container_progress_signal_connected_;
  }

  bool IsImportLxdContainerProgressSignalConnected() override {
    return is_import_lxd_container_progress_signal_connected_;
  }

  bool IsPendingAppListUpdatesSignalConnected() override {
    return is_pending_app_list_updates_signal_connected_;
  }

  bool IsApplyAnsiblePlaybookProgressSignalConnected() override {
    return is_apply_ansible_playbook_progress_signal_connected_;
  }

  bool IsUpgradeContainerProgressSignalConnected() override {
    return is_upgrade_container_progress_signal_connected_;
  }

  bool IsStartLxdProgressSignalConnected() override {
    return is_start_lxd_progress_signal_connected_;
  }

  bool IsFileWatchTriggeredSignalConnected() override {
    return is_file_watch_triggered_signal_connected_;
  }

  bool IsLowDiskSpaceTriggeredSignalConnected() override {
    return is_low_disk_space_triggered_signal_connected_;
  }

  bool IsInhibitScreensaverSignalConencted() override {
    return is_inhibit_screensaver_signal_connected_;
  }

  bool IsUninhibitScreensaverSignalConencted() override {
    return is_uninhibit_screensaver_signal_connected_;
  }

  void LaunchContainerApplication(
      const vm_tools::cicerone::LaunchContainerApplicationRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::LaunchContainerApplicationResponse> callback)
      override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kLaunchContainerApplicationMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR)
          << "Failed to encode LaunchContainerApplicationRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));

      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(
            &CiceroneClientImpl::OnDBusProtoResponse<
                vm_tools::cicerone::LaunchContainerApplicationResponse>,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetContainerAppIcons(
      const vm_tools::cicerone::ContainerAppIconRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::ContainerAppIconResponse>
          callback) override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kGetContainerAppIconMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode ContainerAppIonRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::ContainerAppIconResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetLinuxPackageInfo(
      const vm_tools::cicerone::LinuxPackageInfoRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::LinuxPackageInfoResponse>
          callback) override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kGetLinuxPackageInfoMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode LinuxPackageInfoRequest protobuf";
      std::move(callback).Run(std::nullopt);
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::LinuxPackageInfoResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void InstallLinuxPackage(
      const vm_tools::cicerone::InstallLinuxPackageRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::InstallLinuxPackageResponse> callback) override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kInstallLinuxPackageMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode InstallLinuxPackageRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::InstallLinuxPackageResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UninstallPackageOwningFile(
      const vm_tools::cicerone::UninstallPackageOwningFileRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::UninstallPackageOwningFileResponse> callback)
      override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kUninstallPackageOwningFileMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR)
          << "Failed to encode UninstallPackageOwningFileRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &CiceroneClientImpl::OnDBusProtoResponse<
                vm_tools::cicerone::UninstallPackageOwningFileResponse>,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CreateLxdContainer(
      const vm_tools::cicerone::CreateLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::CreateLxdContainerResponse> callback) override {
    dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                                 vm_tools::cicerone::kCreateLxdContainerMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode CreateLxdContainerRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kLongOperationTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::CreateLxdContainerResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void DeleteLxdContainer(
      const vm_tools::cicerone::DeleteLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::DeleteLxdContainerResponse> callback) override {
    dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                                 vm_tools::cicerone::kDeleteLxdContainerMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode DeleteLxdContainerRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::DeleteLxdContainerResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartLxdContainer(
      const vm_tools::cicerone::StartLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::StartLxdContainerResponse> callback) override {
    dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                                 vm_tools::cicerone::kStartLxdContainerMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode StartLxdContainerRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kLongOperationTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::StartLxdContainerResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StopLxdContainer(
      const vm_tools::cicerone::StopLxdContainerRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::StopLxdContainerResponse>
          callback) override {
    dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                                 vm_tools::cicerone::kStopLxdContainerMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode StopLxdContainerRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kLongOperationTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::StopLxdContainerResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetLxdContainerUsername(
      const vm_tools::cicerone::GetLxdContainerUsernameRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::GetLxdContainerUsernameResponse> callback)
      override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kGetLxdContainerUsernameMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode GetLxdContainerUsernameRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::GetLxdContainerUsernameResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetUpLxdContainerUser(
      const vm_tools::cicerone::SetUpLxdContainerUserRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::SetUpLxdContainerUserResponse> callback)
      override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kSetUpLxdContainerUserMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode SetUpLxdContainerUserRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::SetUpLxdContainerUserResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ExportLxdContainer(
      const vm_tools::cicerone::ExportLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ExportLxdContainerResponse> callback) override {
    dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                                 vm_tools::cicerone::kExportLxdContainerMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode ExportLxdContainerRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::ExportLxdContainerResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ImportLxdContainer(
      const vm_tools::cicerone::ImportLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ImportLxdContainerResponse> callback) override {
    dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                                 vm_tools::cicerone::kImportLxdContainerMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode ImportLxdContainerRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::ImportLxdContainerResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CancelExportLxdContainer(
      const vm_tools::cicerone::CancelExportLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::CancelExportLxdContainerResponse> callback)
      override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kCancelExportLxdContainerMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode CancelExportLxdContainerRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(
            &CiceroneClientImpl::OnDBusProtoResponse<
                vm_tools::cicerone::CancelExportLxdContainerResponse>,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CancelImportLxdContainer(
      const vm_tools::cicerone::CancelImportLxdContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::CancelImportLxdContainerResponse> callback)
      override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kCancelImportLxdContainerMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode CancelImportLxdContainerRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(
            &CiceroneClientImpl::OnDBusProtoResponse<
                vm_tools::cicerone::CancelImportLxdContainerResponse>,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ApplyAnsiblePlaybook(
      const vm_tools::cicerone::ApplyAnsiblePlaybookRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ApplyAnsiblePlaybookResponse> callback) override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kApplyAnsiblePlaybookMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode ApplyAnsiblePlaybookRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::ApplyAnsiblePlaybookResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ConfigureForArcSideload(
      const vm_tools::cicerone::ConfigureForArcSideloadRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ConfigureForArcSideloadResponse> callback)
      override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kConfigureForArcSideloadMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode ConfigureForArcSideloadRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::ConfigureForArcSideloadResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UpgradeContainer(
      const vm_tools::cicerone::UpgradeContainerRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::UpgradeContainerResponse>
          callback) override {
    dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                                 vm_tools::cicerone::kUpgradeContainerMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode UpgradeContainerRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::UpgradeContainerResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void CancelUpgradeContainer(
      const vm_tools::cicerone::CancelUpgradeContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::CancelUpgradeContainerResponse> callback)
      override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kCancelUpgradeContainerMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode CancelUpgradeContainerRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::CancelUpgradeContainerResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartLxd(
      const vm_tools::cicerone::StartLxdRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::StartLxdResponse>
          callback) override {
    dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                                 vm_tools::cicerone::kStartLxdMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode StartLxdRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::StartLxdResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void AddFileWatch(
      const vm_tools::cicerone::AddFileWatchRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::AddFileWatchResponse>
          callback) override {
    dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                                 vm_tools::cicerone::kAddFileWatchMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode AddFileWatchRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::AddFileWatchResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RemoveFileWatch(
      const vm_tools::cicerone::RemoveFileWatchRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::RemoveFileWatchResponse>
          callback) override {
    dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                                 vm_tools::cicerone::kRemoveFileWatchMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode RemoveFileWatchRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::RemoveFileWatchResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetVshSession(
      const vm_tools::cicerone::GetVshSessionRequest& request,
      chromeos::DBusMethodCallback<vm_tools::cicerone::GetVshSessionResponse>
          callback) override {
    dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                                 vm_tools::cicerone::kGetVshSessionMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode GetVshSessionRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::GetVshSessionResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void AttachUsbToContainer(
      const vm_tools::cicerone::AttachUsbToContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::AttachUsbToContainerResponse> callback) override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kAttachUsbToContainerMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode AttachUsbToContainerRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::AttachUsbToContainerResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void DetachUsbFromContainer(
      const vm_tools::cicerone::DetachUsbFromContainerRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::DetachUsbFromContainerResponse> callback)
      override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kDetachUsbFromContainerMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode DetachUsbFromContainerRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::DetachUsbFromContainerResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void FileSelected(
      const vm_tools::cicerone::FileSelectedSignal& signal) override {
    dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                                 vm_tools::cicerone::kFileSelectedMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(signal)) {
      LOG(ERROR) << "Failed to encode FileSelected protobuf";
      return;
    }

    cicerone_proxy_->CallMethod(&method_call, kDefaultTimeout.InMilliseconds(),
                                base::DoNothing());
  }

  void ListRunningContainers(
      const vm_tools::cicerone::ListRunningContainersRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::ListRunningContainersResponse> callback)
      override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kListRunningContainersMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode GetVshSessionRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::ListRunningContainersResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void GetGarconSessionInfo(
      const vm_tools::cicerone::GetGarconSessionInfoRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::GetGarconSessionInfoResponse> callback) override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kGetGarconSessionInfoMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode GetVshSessionRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::GetGarconSessionInfoResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UpdateContainerDevices(
      const vm_tools::cicerone::UpdateContainerDevicesRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::cicerone::UpdateContainerDevicesResponse> callback)
      override {
    dbus::MethodCall method_call(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kUpdateContainerDevicesMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode UpdateContainerDevicesRequest protobuf";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    cicerone_proxy_->CallMethod(
        &method_call, kDefaultTimeout.InMilliseconds(),
        base::BindOnce(&CiceroneClientImpl::OnDBusProtoResponse<
                           vm_tools::cicerone::UpdateContainerDevicesResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    cicerone_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void Init(dbus::Bus* bus) override {
    cicerone_proxy_ = bus->GetObjectProxy(
        vm_tools::cicerone::kVmCiceroneServiceName,
        dbus::ObjectPath(vm_tools::cicerone::kVmCiceroneServicePath));
    if (!cicerone_proxy_) {
      LOG(ERROR) << "Unable to get dbus proxy for "
                 << vm_tools::cicerone::kVmCiceroneServiceName;
    }
    cicerone_proxy_->SetNameOwnerChangedCallback(
        base::BindRepeating(&CiceroneClientImpl::NameOwnerChangedReceived,
                            weak_ptr_factory_.GetWeakPtr()));
    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kContainerStartedSignal,
        base::BindRepeating(&CiceroneClientImpl::OnContainerStartedSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kContainerShutdownSignal,
        base::BindRepeating(&CiceroneClientImpl::OnContainerShutdownSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kInstallLinuxPackageProgressSignal,
        base::BindRepeating(
            &CiceroneClientImpl::OnInstallLinuxPackageProgressSignal,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kUninstallPackageProgressSignal,
        base::BindRepeating(
            &CiceroneClientImpl::OnUninstallPackageProgressSignal,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kLxdContainerCreatedSignal,
        base::BindRepeating(&CiceroneClientImpl::OnLxdContainerCreatedSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kLxdContainerDeletedSignal,
        base::BindRepeating(&CiceroneClientImpl::OnLxdContainerDeletedSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kLxdContainerDownloadingSignal,
        base::BindRepeating(
            &CiceroneClientImpl::OnLxdContainerDownloadingSignal,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kTremplinStartedSignal,
        base::BindRepeating(&CiceroneClientImpl::OnTremplinStartedSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kLxdContainerStartingSignal,
        base::BindRepeating(&CiceroneClientImpl::OnLxdContainerStartingSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kExportLxdContainerProgressSignal,
        base::BindRepeating(
            &CiceroneClientImpl::OnExportLxdContainerProgressSignal,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kImportLxdContainerProgressSignal,
        base::BindRepeating(
            &CiceroneClientImpl::OnImportLxdContainerProgressSignal,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kPendingAppListUpdatesSignal,
        base::BindRepeating(&CiceroneClientImpl::OnPendingAppListUpdatesSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kApplyAnsiblePlaybookProgressSignal,
        base::BindRepeating(
            &CiceroneClientImpl::OnApplyAnsiblePlaybookProgressSignal,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kUpgradeContainerProgressSignal,
        base::BindRepeating(
            &CiceroneClientImpl::OnUpgradeContainerProgressSignal,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kStartLxdProgressSignal,
        base::BindRepeating(&CiceroneClientImpl::OnStartLxdProgressSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kFileWatchTriggeredSignal,
        base::BindRepeating(&CiceroneClientImpl::OnFileWatchTriggeredSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kLowDiskSpaceTriggeredSignal,
        base::BindRepeating(&CiceroneClientImpl::OnLowDiskSpaceTriggeredSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kInhibitScreensaverSignal,
        base::BindRepeating(&CiceroneClientImpl::OnInhibitScreensaverSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    cicerone_proxy_->ConnectToSignal(
        vm_tools::cicerone::kVmCiceroneInterface,
        vm_tools::cicerone::kUninhibitScreensaverSignal,
        base::BindRepeating(&CiceroneClientImpl::OnUninhibitScreensaverSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&CiceroneClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  template <typename ResponseProto>
  void OnDBusProtoResponse(chromeos::DBusMethodCallback<ResponseProto> callback,
                           dbus::Response* dbus_response) {
    if (!dbus_response) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    ResponseProto reponse_proto;
    dbus::MessageReader reader(dbus_response);
    if (!reader.PopArrayOfBytesAsProto(&reponse_proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Response.";
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(std::move(reponse_proto));
  }

  void NameOwnerChangedReceived(const std::string& old_owner,
                                const std::string& new_owner) {
    if (!old_owner.empty()) {
      for (auto& observer : observer_list_) {
        observer.CiceroneServiceStopped();
      }
    }
    if (!new_owner.empty()) {
      for (auto& observer : observer_list_) {
        observer.CiceroneServiceStarted();
      }
    }
  }

  void OnContainerStartedSignal(dbus::Signal* signal) {
    vm_tools::cicerone::ContainerStartedSignal container_started_signal;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&container_started_signal)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnContainerStarted(container_started_signal);
    }
  }

  void OnContainerShutdownSignal(dbus::Signal* signal) {
    vm_tools::cicerone::ContainerShutdownSignal container_shutdown_signal;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&container_shutdown_signal)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnContainerShutdown(container_shutdown_signal);
    }
  }

  void OnInstallLinuxPackageProgressSignal(dbus::Signal* signal) {
    vm_tools::cicerone::InstallLinuxPackageProgressSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnInstallLinuxPackageProgress(proto);
    }
  }

  void OnUninstallPackageProgressSignal(dbus::Signal* signal) {
    vm_tools::cicerone::UninstallPackageProgressSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnUninstallPackageProgress(proto);
    }
  }

  void OnLxdContainerCreatedSignal(dbus::Signal* signal) {
    vm_tools::cicerone::LxdContainerCreatedSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnLxdContainerCreated(proto);
    }
  }

  void OnLxdContainerDeletedSignal(dbus::Signal* signal) {
    vm_tools::cicerone::LxdContainerDeletedSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnLxdContainerDeleted(proto);
    }
  }

  void OnLxdContainerDownloadingSignal(dbus::Signal* signal) {
    vm_tools::cicerone::LxdContainerDownloadingSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnLxdContainerDownloading(proto);
    }
  }

  void OnTremplinStartedSignal(dbus::Signal* signal) {
    vm_tools::cicerone::TremplinStartedSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnTremplinStarted(proto);
    }
  }

  void OnLxdContainerStartingSignal(dbus::Signal* signal) {
    vm_tools::cicerone::LxdContainerStartingSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnLxdContainerStarting(proto);
    }
  }

  void OnExportLxdContainerProgressSignal(dbus::Signal* signal) {
    vm_tools::cicerone::ExportLxdContainerProgressSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnExportLxdContainerProgress(proto);
    }
  }

  void OnImportLxdContainerProgressSignal(dbus::Signal* signal) {
    vm_tools::cicerone::ImportLxdContainerProgressSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnImportLxdContainerProgress(proto);
    }
  }

  void OnPendingAppListUpdatesSignal(dbus::Signal* signal) {
    vm_tools::cicerone::PendingAppListUpdatesSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnPendingAppListUpdates(proto);
    }
  }

  void OnApplyAnsiblePlaybookProgressSignal(dbus::Signal* signal) {
    vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnApplyAnsiblePlaybookProgress(proto);
    }
  }

  void OnUpgradeContainerProgressSignal(dbus::Signal* signal) {
    vm_tools::cicerone::UpgradeContainerProgressSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnUpgradeContainerProgress(proto);
    }
  }

  void OnStartLxdProgressSignal(dbus::Signal* signal) {
    vm_tools::cicerone::StartLxdProgressSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnStartLxdProgress(proto);
    }
  }

  void OnFileWatchTriggeredSignal(dbus::Signal* signal) {
    vm_tools::cicerone::FileWatchTriggeredSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnFileWatchTriggered(proto);
    }
  }

  void OnLowDiskSpaceTriggeredSignal(dbus::Signal* signal) {
    vm_tools::cicerone::LowDiskSpaceTriggeredSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnLowDiskSpaceTriggered(proto);
    }
  }

  void OnInhibitScreensaverSignal(dbus::Signal* signal) {
    vm_tools::cicerone::InhibitScreensaverSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnInhibitScreensaver(proto);
    }
  }

  void OnUninhibitScreensaverSignal(dbus::Signal* signal) {
    vm_tools::cicerone::UninhibitScreensaverSignal proto;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnUninhibitScreensaver(proto);
    }
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool is_connected) {
    if (!is_connected) {
      LOG(ERROR)
          << "Failed to connect to Signal. Async container ops may not work";
    }
    DCHECK_EQ(interface_name, vm_tools::cicerone::kVmCiceroneInterface);
    if (signal_name == vm_tools::cicerone::kContainerStartedSignal) {
      is_container_started_signal_connected_ = is_connected;
    } else if (signal_name == vm_tools::cicerone::kContainerShutdownSignal) {
      is_container_shutdown_signal_connected_ = is_connected;
    } else if (signal_name ==
               vm_tools::cicerone::kInstallLinuxPackageProgressSignal) {
      is_install_linux_package_progress_signal_connected_ = is_connected;
    } else if (signal_name ==
               vm_tools::cicerone::kUninstallPackageProgressSignal) {
      is_uninstall_package_progress_signal_connected_ = is_connected;
    } else if (signal_name == vm_tools::cicerone::kLxdContainerCreatedSignal) {
      is_lxd_container_created_signal_connected_ = is_connected;
    } else if (signal_name == vm_tools::cicerone::kLxdContainerDeletedSignal) {
      is_lxd_container_deleted_signal_connected_ = is_connected;
    } else if (signal_name ==
               vm_tools::cicerone::kLxdContainerDownloadingSignal) {
      is_lxd_container_downloading_signal_connected_ = is_connected;
    } else if (signal_name == vm_tools::cicerone::kTremplinStartedSignal) {
      is_tremplin_started_signal_connected_ = is_connected;
    } else if (signal_name == vm_tools::cicerone::kLxdContainerStartingSignal) {
      is_lxd_container_starting_signal_connected_ = is_connected;
    } else if (signal_name ==
               vm_tools::cicerone::kExportLxdContainerProgressSignal) {
      is_export_lxd_container_progress_signal_connected_ = is_connected;
    } else if (signal_name ==
               vm_tools::cicerone::kImportLxdContainerProgressSignal) {
      is_import_lxd_container_progress_signal_connected_ = is_connected;
    } else if (signal_name ==
               vm_tools::cicerone::kPendingAppListUpdatesSignal) {
      is_pending_app_list_updates_signal_connected_ = is_connected;
    } else if (signal_name ==
               vm_tools::cicerone::kApplyAnsiblePlaybookProgressSignal) {
      is_apply_ansible_playbook_progress_signal_connected_ = is_connected;
    } else if (signal_name ==
               vm_tools::cicerone::kUpgradeContainerProgressSignal) {
      is_upgrade_container_progress_signal_connected_ = is_connected;
    } else if (signal_name == vm_tools::cicerone::kStartLxdProgressSignal) {
      is_start_lxd_progress_signal_connected_ = is_connected;
    } else if (signal_name == vm_tools::cicerone::kFileWatchTriggeredSignal) {
      is_file_watch_triggered_signal_connected_ = is_connected;
    } else if (signal_name ==
               vm_tools::cicerone::kLowDiskSpaceTriggeredSignal) {
      is_low_disk_space_triggered_signal_connected_ = is_connected;
    } else if (signal_name == vm_tools::cicerone::kInhibitScreensaverSignal) {
      is_inhibit_screensaver_signal_connected_ = is_connected;
    } else if (signal_name == vm_tools::cicerone::kUninhibitScreensaverSignal) {
      is_uninhibit_screensaver_signal_connected_ = is_connected;
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  raw_ptr<dbus::ObjectProxy> cicerone_proxy_ = nullptr;

  base::ObserverList<Observer>::Unchecked observer_list_;

  bool is_container_started_signal_connected_ = false;
  bool is_container_shutdown_signal_connected_ = false;
  bool is_install_linux_package_progress_signal_connected_ = false;
  bool is_uninstall_package_progress_signal_connected_ = false;
  bool is_lxd_container_created_signal_connected_ = false;
  bool is_lxd_container_deleted_signal_connected_ = false;
  bool is_lxd_container_downloading_signal_connected_ = false;
  bool is_tremplin_started_signal_connected_ = false;
  bool is_lxd_container_starting_signal_connected_ = false;
  bool is_export_lxd_container_progress_signal_connected_ = false;
  bool is_import_lxd_container_progress_signal_connected_ = false;
  bool is_pending_app_list_updates_signal_connected_ = false;
  bool is_apply_ansible_playbook_progress_signal_connected_ = false;
  bool is_upgrade_container_progress_signal_connected_ = false;
  bool is_start_lxd_progress_signal_connected_ = false;
  bool is_file_watch_triggered_signal_connected_ = false;
  bool is_low_disk_space_triggered_signal_connected_ = false;
  bool is_inhibit_screensaver_signal_connected_ = false;
  bool is_uninhibit_screensaver_signal_connected_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CiceroneClientImpl> weak_ptr_factory_{this};
};

CiceroneClient::CiceroneClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

CiceroneClient::~CiceroneClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void CiceroneClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new CiceroneClientImpl())->Init(bus);
}

// static
void CiceroneClient::InitializeFake() {
  // Do not create a new fake if it was initialized early in a browser test to
  // allow the test to set its own client.
  if (!FakeCiceroneClient::Get()) {
    new FakeCiceroneClient();
  }
}

// static
void CiceroneClient::Shutdown() {
  delete g_instance;
}

// static
CiceroneClient* CiceroneClient::Get() {
  return g_instance;
}

}  // namespace ash
