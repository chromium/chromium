// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/concierge_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/vm_concierge/dbus-constants.h"

namespace concierge = vm_tools::concierge;

namespace {

// TODO(nverne): revert to TIMEOUT_USE_DEFAULT when StartVm no longer requires
// unnecessary long running crypto calculations _and_ b/143499148 is fixed.
// TODO(yusukes): Fix b/143499148.
constexpr int kConciergeDBusTimeoutMs = 160 * 1000;

}  // namespace

namespace chromeos {

class ConciergeClientImpl : public ConciergeClient {
 public:
  ConciergeClientImpl() = default;

  ~ConciergeClientImpl() override = default;

  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void AddVmObserver(VmObserver* observer) override {
    vm_observer_list_.AddObserver(observer);
  }

  void RemoveVmObserver(VmObserver* observer) override {
    vm_observer_list_.RemoveObserver(observer);
  }

  void AddContainerObserver(ContainerObserver* observer) override {
    container_observer_list_.AddObserver(observer);
  }

  void RemoveContainerObserver(ContainerObserver* observer) override {
    container_observer_list_.RemoveObserver(observer);
  }

  void AddDiskImageObserver(DiskImageObserver* observer) override {
    disk_image_observer_list_.AddObserver(observer);
  }

  void RemoveDiskImageObserver(DiskImageObserver* observer) override {
    disk_image_observer_list_.RemoveObserver(observer);
  }

  bool IsVmStartedSignalConnected() override {
    return is_vm_started_signal_connected_;
  }

  bool IsVmStoppedSignalConnected() override {
    return is_vm_stopped_signal_connected_;
  }

  bool IsContainerStartupFailedSignalConnected() override {
    return is_container_startup_failed_signal_connected_;
  }

  bool IsDiskImageProgressSignalConnected() override {
    return is_disk_import_progress_signal_connected_;
  }

  void CreateDiskImage(const concierge::CreateDiskImageRequest& request,
                       DBusMethodCallback<concierge::CreateDiskImageResponse>
                           callback) override {
    CallMethod(concierge::kCreateDiskImageMethod, request, std::move(callback));
  }

  void CreateDiskImageWithFd(
      base::ScopedFD fd,
      const concierge::CreateDiskImageRequest& request,
      DBusMethodCallback<concierge::CreateDiskImageResponse> callback)
      override {
    CallMethodWithFd(concierge::kCreateDiskImageMethod, request, std::move(fd),
                     std::move(callback));
  }

  void DestroyDiskImage(const concierge::DestroyDiskImageRequest& request,
                        DBusMethodCallback<concierge::DestroyDiskImageResponse>
                            callback) override {
    CallMethod(concierge::kDestroyDiskImageMethod, request,
               std::move(callback));
  }

  void ImportDiskImage(base::ScopedFD fd,
                       const concierge::ImportDiskImageRequest& request,
                       DBusMethodCallback<concierge::ImportDiskImageResponse>
                           callback) override {
    CallMethodWithFd(concierge::kImportDiskImageMethod, request, std::move(fd),
                     std::move(callback));
  }

  void CancelDiskImageOperation(
      const concierge::CancelDiskImageRequest& request,
      DBusMethodCallback<concierge::CancelDiskImageResponse> callback)
      override {
    CallMethod(concierge::kCancelDiskImageMethod, request, std::move(callback));
  }

  void DiskImageStatus(const concierge::DiskImageStatusRequest& request,
                       DBusMethodCallback<concierge::DiskImageStatusResponse>
                           callback) override {
    CallMethod(concierge::kDiskImageStatusMethod, request, std::move(callback));
  }

  void ListVmDisks(
      const concierge::ListVmDisksRequest& request,
      DBusMethodCallback<concierge::ListVmDisksResponse> callback) override {
    CallMethod(concierge::kListVmDisksMethod, request, std::move(callback));
  }

  void StartTerminaVm(
      const concierge::StartVmRequest& request,
      DBusMethodCallback<concierge::StartVmResponse> callback) override {
    CallMethod(concierge::kStartVmMethod, request, std::move(callback));
  }

  void StopVm(const concierge::StopVmRequest& request,
              DBusMethodCallback<concierge::StopVmResponse> callback) override {
    CallMethod(concierge::kStopVmMethod, request, std::move(callback));
  }

  void SuspendVm(
      const concierge::SuspendVmRequest& request,
      DBusMethodCallback<concierge::SuspendVmResponse> callback) override {
    CallMethod(concierge::kSuspendVmMethod, request, std::move(callback));
  }

  void ResumeVm(
      const concierge::ResumeVmRequest& request,
      DBusMethodCallback<concierge::ResumeVmResponse> callback) override {
    CallMethod(concierge::kResumeVmMethod, request, std::move(callback));
  }

  void GetVmInfo(
      const concierge::GetVmInfoRequest& request,
      DBusMethodCallback<concierge::GetVmInfoResponse> callback) override {
    CallMethod(concierge::kGetVmInfoMethod, request, std::move(callback));
  }

  void GetVmEnterpriseReportingInfo(
      const concierge::GetVmEnterpriseReportingInfoRequest& request,
      DBusMethodCallback<concierge::GetVmEnterpriseReportingInfoResponse>
          callback) override {
    CallMethod(concierge::kGetVmEnterpriseReportingInfoMethod, request,
               std::move(callback));
  }

  void SetVmCpuRestriction(
      const concierge::SetVmCpuRestrictionRequest& request,
      DBusMethodCallback<concierge::SetVmCpuRestrictionResponse> callback)
      override {
    CallMethod(concierge::kSetVmCpuRestrictionMethod, request,
               std::move(callback));
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    concierge_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void GetContainerSshKeys(
      const concierge::ContainerSshKeysRequest& request,
      DBusMethodCallback<concierge::ContainerSshKeysResponse> callback)
      override {
    CallMethod(concierge::kGetContainerSshKeysMethod, request,
               std::move(callback));
  }

  void AttachUsbDevice(base::ScopedFD fd,
                       const concierge::AttachUsbDeviceRequest& request,
                       DBusMethodCallback<concierge::AttachUsbDeviceResponse>
                           callback) override {
    dbus::MethodCall method_call(concierge::kVmConciergeInterface,
                                 concierge::kAttachUsbDeviceMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode AttachUsbDeviceRequest protobuf";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    writer.AppendFileDescriptor(fd.get());

    concierge_proxy_->CallMethod(
        &method_call, kConciergeDBusTimeoutMs,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           concierge::AttachUsbDeviceResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void DetachUsbDevice(const concierge::DetachUsbDeviceRequest& request,
                       DBusMethodCallback<concierge::DetachUsbDeviceResponse>
                           callback) override {
    CallMethod(concierge::kDetachUsbDeviceMethod, request, std::move(callback));
  }

  void StartArcVm(
      const concierge::StartArcVmRequest& request,
      DBusMethodCallback<concierge::StartVmResponse> callback) override {
    CallMethod(concierge::kStartArcVmMethod, request, std::move(callback));
  }

  void ResizeDiskImage(const concierge::ResizeDiskImageRequest& request,
                       DBusMethodCallback<concierge::ResizeDiskImageResponse>
                           callback) override {
    CallMethod(concierge::kResizeDiskImageMethod, request, std::move(callback));
  }

  void SetVmId(const vm_tools::concierge::SetVmIdRequest& request,
               DBusMethodCallback<vm_tools::concierge::SetVmIdResponse>
                   callback) override {
    CallMethod(concierge::kSetVmIdMethod, request, std::move(callback));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    concierge_proxy_ = bus->GetObjectProxy(
        concierge::kVmConciergeServiceName,
        dbus::ObjectPath(concierge::kVmConciergeServicePath));
    if (!concierge_proxy_) {
      LOG(ERROR) << "Unable to get dbus proxy for "
                 << concierge::kVmConciergeServiceName;
    }
    concierge_proxy_->SetNameOwnerChangedCallback(
        base::BindRepeating(&ConciergeClientImpl::NameOwnerChangedReceived,
                            weak_ptr_factory_.GetWeakPtr()));
    concierge_proxy_->ConnectToSignal(
        concierge::kVmConciergeInterface, concierge::kVmStartedSignal,
        base::BindRepeating(&ConciergeClientImpl::OnVmStartedSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ConciergeClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    concierge_proxy_->ConnectToSignal(
        concierge::kVmConciergeInterface, concierge::kVmStoppedSignal,
        base::BindRepeating(&ConciergeClientImpl::OnVmStoppedSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ConciergeClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    concierge_proxy_->ConnectToSignal(
        concierge::kVmConciergeInterface,
        concierge::kContainerStartupFailedSignal,
        base::BindRepeating(
            &ConciergeClientImpl::OnContainerStartupFailedSignal,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ConciergeClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    concierge_proxy_->ConnectToSignal(
        concierge::kVmConciergeInterface, concierge::kDiskImageProgressSignal,
        base::BindRepeating(&ConciergeClientImpl::OnDiskImageProgress,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ConciergeClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  template <typename RequestProto, typename ResponseProto>
  void CallMethodWithFd(const std::string& method_name,
                        const RequestProto& request,
                        base::ScopedFD fd,
                        DBusMethodCallback<ResponseProto> callback) {
    dbus::MethodCall method_call(concierge::kVmConciergeInterface, method_name);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode protobuf for " << method_name;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
      return;
    }

    if (fd.is_valid())
      writer.AppendFileDescriptor(fd.get());

    concierge_proxy_->CallMethod(
        &method_call, kConciergeDBusTimeoutMs,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<ResponseProto>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  template <typename RequestProto, typename ResponseProto>
  void CallMethod(const std::string& method_name,
                  const RequestProto& request,
                  DBusMethodCallback<ResponseProto> callback) {
    CallMethodWithFd(method_name, request, base::ScopedFD(),
                     std::move(callback));
  }

  template <typename ResponseProto>
  void OnDBusProtoResponse(DBusMethodCallback<ResponseProto> callback,
                           dbus::Response* dbus_response) {
    if (!dbus_response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    ResponseProto reponse_proto;
    dbus::MessageReader reader(dbus_response);
    if (!reader.PopArrayOfBytesAsProto(&reponse_proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus Response.";
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(std::move(reponse_proto));
  }

  void NameOwnerChangedReceived(const std::string& old_owner,
                                const std::string& new_owner) {
    if (!old_owner.empty()) {
      for (auto& observer : observer_list_) {
        observer.ConciergeServiceStopped();
      }
    }
    if (!new_owner.empty()) {
      for (auto& observer : observer_list_) {
        observer.ConciergeServiceStarted();
      }
    }
  }

  void OnVmStartedSignal(dbus::Signal* signal) {
    DCHECK_EQ(signal->GetInterface(), concierge::kVmConciergeInterface);
    DCHECK_EQ(signal->GetMember(), concierge::kVmStartedSignal);

    concierge::VmStartedSignal vm_started_signal;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&vm_started_signal)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }

    for (auto& observer : vm_observer_list_)
      observer.OnVmStarted(vm_started_signal);
  }

  void OnVmStoppedSignal(dbus::Signal* signal) {
    DCHECK_EQ(signal->GetInterface(), concierge::kVmConciergeInterface);
    DCHECK_EQ(signal->GetMember(), concierge::kVmStoppedSignal);

    concierge::VmStoppedSignal vm_stopped_signal;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&vm_stopped_signal)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }

    for (auto& observer : vm_observer_list_)
      observer.OnVmStopped(vm_stopped_signal);
  }

  void OnContainerStartupFailedSignal(dbus::Signal* signal) {
    DCHECK_EQ(signal->GetInterface(), concierge::kVmConciergeInterface);
    DCHECK_EQ(signal->GetMember(), concierge::kContainerStartupFailedSignal);

    concierge::ContainerStartedSignal container_startup_failed_signal;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&container_startup_failed_signal)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }

    for (auto& observer : container_observer_list_) {
      observer.OnContainerStartupFailed(container_startup_failed_signal);
    }
  }

  void OnDiskImageProgress(dbus::Signal* signal) {
    DCHECK_EQ(signal->GetInterface(), concierge::kVmConciergeInterface);
    DCHECK_EQ(signal->GetMember(), concierge::kDiskImageProgressSignal);

    concierge::DiskImageStatusResponse disk_image_status_response_signal;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&disk_image_status_response_signal)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }

    for (auto& observer : disk_image_observer_list_) {
      observer.OnDiskImageProgress(disk_image_status_response_signal);
    }
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool is_connected) {
    DCHECK_EQ(interface_name, concierge::kVmConciergeInterface);
    if (!is_connected)
      LOG(ERROR) << "Failed to connect to signal: " << signal_name;

    if (signal_name == concierge::kVmStartedSignal) {
      is_vm_started_signal_connected_ = is_connected;
    } else if (signal_name == concierge::kVmStoppedSignal) {
      is_vm_stopped_signal_connected_ = is_connected;
    } else if (signal_name == concierge::kContainerStartupFailedSignal) {
      is_container_startup_failed_signal_connected_ = is_connected;
    } else if (signal_name == concierge::kDiskImageProgressSignal) {
      is_disk_import_progress_signal_connected_ = is_connected;
    } else {
      NOTREACHED();
    }
  }

  dbus::ObjectProxy* concierge_proxy_ = nullptr;

  base::ObserverList<Observer> observer_list_{
      base::ObserverListPolicy::EXISTING_ONLY};
  base::ObserverList<VmObserver>::Unchecked vm_observer_list_{
      base::ObserverListPolicy::EXISTING_ONLY};
  base::ObserverList<ContainerObserver>::Unchecked container_observer_list_{
      base::ObserverListPolicy::EXISTING_ONLY};
  base::ObserverList<DiskImageObserver>::Unchecked disk_image_observer_list_{
      base::ObserverListPolicy::EXISTING_ONLY};

  bool is_vm_started_signal_connected_ = false;
  bool is_vm_stopped_signal_connected_ = false;
  bool is_container_startup_failed_signal_connected_ = false;
  bool is_disk_import_progress_signal_connected_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ConciergeClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ConciergeClientImpl);
};

ConciergeClient::ConciergeClient() = default;

ConciergeClient::~ConciergeClient() = default;

std::unique_ptr<ConciergeClient> ConciergeClient::Create() {
  return std::make_unique<ConciergeClientImpl>();
}

}  // namespace chromeos
