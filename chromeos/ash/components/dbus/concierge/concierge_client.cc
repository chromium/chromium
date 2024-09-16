// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/concierge/concierge_client.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/vm_concierge/dbus-constants.h"

namespace concierge = vm_tools::concierge;

namespace ash {

namespace {

ConciergeClient* g_instance = nullptr;

// TODO(nverne): revert to TIMEOUT_USE_DEFAULT when StartVm no longer requires
// unnecessary long running crypto calculations.
constexpr int kConciergeDBusTimeoutMs = 300 * 1000;

}  // namespace

class ConciergeClientImpl : public ConciergeClient {
 public:
  ConciergeClientImpl() = default;

  ConciergeClientImpl(const ConciergeClientImpl&) = delete;
  ConciergeClientImpl& operator=(const ConciergeClientImpl&) = delete;

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

  bool IsVmStoppingSignalConnected() override {
    return is_vm_stopping_signal_connected_;
  }

  bool IsDiskImageProgressSignalConnected() override {
    return is_disk_image_progress_signal_connected_;
  }

  void CreateDiskImage(
      const concierge::CreateDiskImageRequest& request,
      chromeos::DBusMethodCallback<concierge::CreateDiskImageResponse> callback)
      override {
    CallMethodWithFds(concierge::kCreateDiskImageMethod, request,
                      std::vector<base::ScopedFD>(), std::move(callback));
  }

  void CreateDiskImageWithFd(
      base::ScopedFD fd,
      const concierge::CreateDiskImageRequest& request,
      chromeos::DBusMethodCallback<concierge::CreateDiskImageResponse> callback)
      override {
    std::vector<base::ScopedFD> fds;
    fds.emplace_back(std::move(fd));

    CallMethodWithFds(concierge::kCreateDiskImageMethod, request,
                      std::move(fds), std::move(callback));
  }

  void DestroyDiskImage(
      const concierge::DestroyDiskImageRequest& request,
      chromeos::DBusMethodCallback<concierge::DestroyDiskImageResponse>
          callback) override {
    CallMethod(concierge::kDestroyDiskImageMethod, request,
               std::move(callback));
  }

  void ImportDiskImage(
      base::ScopedFD fd,
      const concierge::ImportDiskImageRequest& request,
      chromeos::DBusMethodCallback<concierge::ImportDiskImageResponse> callback)
      override {
    CallMethodWithFd(concierge::kImportDiskImageMethod, request, std::move(fd),
                     std::move(callback));
  }

  void ExportDiskImage(
      std::vector<base::ScopedFD> fds,
      const concierge::ExportDiskImageRequest& request,
      chromeos::DBusMethodCallback<concierge::ExportDiskImageResponse> callback)
      override {
    CallMethodWithFds(concierge::kExportDiskImageMethod, request,
                      std::move(fds), std::move(callback));
  }

  void CancelDiskImageOperation(
      const concierge::CancelDiskImageRequest& request,
      chromeos::DBusMethodCallback<concierge::CancelDiskImageResponse> callback)
      override {
    CallMethod(concierge::kCancelDiskImageMethod, request, std::move(callback));
  }

  void DiskImageStatus(
      const concierge::DiskImageStatusRequest& request,
      chromeos::DBusMethodCallback<concierge::DiskImageStatusResponse> callback)
      override {
    CallMethod(concierge::kDiskImageStatusMethod, request, std::move(callback));
  }

  void ListVmDisks(const concierge::ListVmDisksRequest& request,
                   chromeos::DBusMethodCallback<concierge::ListVmDisksResponse>
                       callback) override {
    CallMethod(concierge::kListVmDisksMethod, request, std::move(callback));
  }

  void StartVm(const concierge::StartVmRequest& request,
               chromeos::DBusMethodCallback<concierge::StartVmResponse>
                   callback) override {
    CallMethodWithFds(concierge::kStartVmMethod, request, {},
                      std::move(callback));
  }

  void StartVmWithFd(
      base::ScopedFD fd,
      const vm_tools::concierge::StartVmRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
          callback) override {
    std::vector<base::ScopedFD> fds;
    fds.emplace_back(std::move(fd));
    CallMethodWithFds(concierge::kStartVmMethod, request, std::move(fds),
                      std::move(callback));
  }

  void StopVm(const concierge::StopVmRequest& request,
              chromeos::DBusMethodCallback<concierge::StopVmResponse> callback)
      override {
    CallMethod(concierge::kStopVmMethod, request, std::move(callback));
  }

  void SuspendVm(const concierge::SuspendVmRequest& request,
                 chromeos::DBusMethodCallback<concierge::SuspendVmResponse>
                     callback) override {
    CallMethod(concierge::kSuspendVmMethod, request, std::move(callback));
  }

  void ResumeVm(const concierge::ResumeVmRequest& request,
                chromeos::DBusMethodCallback<concierge::ResumeVmResponse>
                    callback) override {
    CallMethod(concierge::kResumeVmMethod, request, std::move(callback));
  }

  void GetVmInfo(const concierge::GetVmInfoRequest& request,
                 chromeos::DBusMethodCallback<concierge::GetVmInfoResponse>
                     callback) override {
    CallMethod(concierge::kGetVmInfoMethod, request, std::move(callback));
  }

  void GetVmEnterpriseReportingInfo(
      const concierge::GetVmEnterpriseReportingInfoRequest& request,
      chromeos::DBusMethodCallback<
          concierge::GetVmEnterpriseReportingInfoResponse> callback) override {
    CallMethod(concierge::kGetVmEnterpriseReportingInfoMethod, request,
               std::move(callback));
  }

  void ArcVmCompleteBoot(
      const vm_tools::concierge::ArcVmCompleteBootRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::ArcVmCompleteBootResponse> callback) override {
    CallMethod(concierge::kArcVmCompleteBootMethod, request,
               std::move(callback));
  }

  void SetVmCpuRestriction(
      const concierge::SetVmCpuRestrictionRequest& request,
      chromeos::DBusMethodCallback<concierge::SetVmCpuRestrictionResponse>
          callback) override {
    CallMethod(concierge::kSetVmCpuRestrictionMethod, request,
               std::move(callback));
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    concierge_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void AttachUsbDevice(
      base::ScopedFD fd,
      const concierge::AttachUsbDeviceRequest& request,
      chromeos::DBusMethodCallback<concierge::AttachUsbDeviceResponse> callback)
      override {
    CallMethodWithFd(concierge::kAttachUsbDeviceMethod, request, std::move(fd),
                     std::move(callback));
  }

  void DetachUsbDevice(
      const concierge::DetachUsbDeviceRequest& request,
      chromeos::DBusMethodCallback<concierge::DetachUsbDeviceResponse> callback)
      override {
    CallMethod(concierge::kDetachUsbDeviceMethod, request, std::move(callback));
  }

  void StartArcVm(const concierge::StartArcVmRequest& request,
                  chromeos::DBusMethodCallback<concierge::StartVmResponse>
                      callback) override {
    CallMethod(concierge::kStartArcVmMethod, request, std::move(callback));
  }

  void ResizeDiskImage(
      const concierge::ResizeDiskImageRequest& request,
      chromeos::DBusMethodCallback<concierge::ResizeDiskImageResponse> callback)
      override {
    CallMethod(concierge::kResizeDiskImageMethod, request, std::move(callback));
  }

  void ReclaimVmMemory(
      const vm_tools::concierge::ReclaimVmMemoryRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::ReclaimVmMemoryResponse>
          callback) override {
    CallMethod(concierge::kReclaimVmMemoryMethod, request, std::move(callback));
  }

  void ListVms(const concierge::ListVmsRequest& request,
               chromeos::DBusMethodCallback<concierge::ListVmsResponse>
                   callback) override {
    CallMethod(concierge::kListVmsMethod, request, std::move(callback));
  }

  void GetVmLaunchAllowed(
      const vm_tools::concierge::GetVmLaunchAllowedRequest& request,
      chromeos::DBusMethodCallback<concierge::GetVmLaunchAllowedResponse>
          callback) override {
    CallMethod(concierge::kGetVmLaunchAllowedMethod, request,
               std::move(callback));
  }

  void SwapVm(const vm_tools::concierge::SwapVmRequest& request,
              chromeos::DBusMethodCallback<vm_tools::concierge::SwapVmResponse>
                  callback) override {
    CallMethod(concierge::kSwapVmMethod, request, std::move(callback));
  }

  void InstallPflash(
      base::ScopedFD fd,
      const vm_tools::concierge::InstallPflashRequest& request,
      chromeos::DBusMethodCallback<vm_tools::concierge::InstallPflashResponse>
          callback) override {
    CallMethodWithFd(concierge::kInstallPflashMethod, request, std::move(fd),
                     std::move(callback));
  }

  void AggressiveBalloon(
      const vm_tools::concierge::AggressiveBalloonRequest& request,
      chromeos::DBusMethodCallback<
          vm_tools::concierge::AggressiveBalloonResponse> callback) override {
    CallMethod(concierge::kAggressiveBalloonMethod, request,
               std::move(callback));
  }

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
        concierge::kVmConciergeInterface, concierge::kVmStoppingSignal,
        base::BindRepeating(&ConciergeClientImpl::OnVmStoppingSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ConciergeClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
    concierge_proxy_->ConnectToSignal(
        concierge::kVmConciergeInterface, concierge::kVmSwappingSignal,
        base::BindRepeating(&ConciergeClientImpl::OnVmSwappingSignal,
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
  void CallMethodWithFds(const std::string& method_name,
                         const RequestProto& request,
                         std::vector<base::ScopedFD> fds,
                         chromeos::DBusMethodCallback<ResponseProto> callback) {
    dbus::MethodCall method_call(concierge::kVmConciergeInterface, method_name);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode protobuf for " << method_name;
      std::move(callback).Run(std::nullopt);
      return;
    }

    {
      dbus::MessageWriter array_writer(nullptr);

      writer.OpenArray("h", &array_writer);
      for (const auto& fd : fds) {
        array_writer.AppendFileDescriptor(fd.get());
      }
      writer.CloseContainer(&array_writer);
    }

    concierge_proxy_->CallMethod(
        &method_call, kConciergeDBusTimeoutMs,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<ResponseProto>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  template <typename RequestProto, typename ResponseProto>
  void CallMethodWithFd(const std::string& method_name,
                        const RequestProto& request,
                        base::ScopedFD fd,
                        chromeos::DBusMethodCallback<ResponseProto> callback) {
    dbus::MethodCall method_call(concierge::kVmConciergeInterface, method_name);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode protobuf for " << method_name;
      std::move(callback).Run(std::nullopt);
      return;
    }

    writer.AppendFileDescriptor(fd.get());

    concierge_proxy_->CallMethod(
        &method_call, kConciergeDBusTimeoutMs,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<ResponseProto>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  template <typename RequestProto, typename ResponseProto>
  void CallMethod(const std::string& method_name,
                  const RequestProto& request,
                  chromeos::DBusMethodCallback<ResponseProto> callback) {
    dbus::MethodCall method_call(concierge::kVmConciergeInterface, method_name);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode protobuf for " << method_name;
      // TODO(uekawa): Check if posttask is needed.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, kConciergeDBusTimeoutMs,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<ResponseProto>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

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

    for (auto& observer : vm_observer_list_) {
      observer.OnVmStarted(vm_started_signal);
    }
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

    for (auto& observer : vm_observer_list_) {
      observer.OnVmStopped(vm_stopped_signal);
    }
  }

  void OnVmStoppingSignal(dbus::Signal* signal) {
    DCHECK_EQ(signal->GetInterface(), concierge::kVmConciergeInterface);
    DCHECK_EQ(signal->GetMember(), concierge::kVmStoppingSignal);

    concierge::VmStoppingSignal vm_stopping_signal;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&vm_stopping_signal)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }

    for (auto& observer : vm_observer_list_) {
      observer.OnVmStopping(vm_stopping_signal);
    }
  }

  void OnVmSwappingSignal(dbus::Signal* signal) {
    DCHECK_EQ(signal->GetInterface(), concierge::kVmConciergeInterface);
    DCHECK_EQ(signal->GetMember(), concierge::kVmSwappingSignal);

    concierge::VmSwappingSignal vm_swapping_signal;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&vm_swapping_signal)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }

    for (auto& observer : vm_observer_list_) {
      observer.OnVmSwapping(vm_swapping_signal);
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
    if (!is_connected) {
      LOG(ERROR) << "Failed to connect to signal: " << signal_name;
    }

    if (signal_name == concierge::kVmStartedSignal) {
      is_vm_started_signal_connected_ = is_connected;
    } else if (signal_name == concierge::kVmStoppedSignal) {
      is_vm_stopped_signal_connected_ = is_connected;
    } else if (signal_name == concierge::kDiskImageProgressSignal) {
      is_disk_image_progress_signal_connected_ = is_connected;
    } else if (signal_name == concierge::kVmStoppingSignal) {
      is_vm_stopping_signal_connected_ = is_connected;
    } else if (signal_name == concierge::kVmSwappingSignal) {
      // DO NOTHING.
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  raw_ptr<dbus::ObjectProxy> concierge_proxy_ = nullptr;

  base::ObserverList<Observer> observer_list_{
      ConciergeClient::kObserverListPolicy};
  base::ObserverList<VmObserver>::UncheckedAndDanglingUntriaged
      vm_observer_list_{ConciergeClient::kObserverListPolicy};
  base::ObserverList<DiskImageObserver>::UncheckedAndDanglingUntriaged
      disk_image_observer_list_{ConciergeClient::kObserverListPolicy};

  bool is_vm_started_signal_connected_ = false;
  bool is_vm_stopped_signal_connected_ = false;
  bool is_vm_stopping_signal_connected_ = false;
  bool is_disk_image_progress_signal_connected_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ConciergeClientImpl> weak_ptr_factory_{this};
};

ConciergeClient::ConciergeClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

ConciergeClient::~ConciergeClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ConciergeClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new ConciergeClientImpl())->Init(bus);
}

// static
void ConciergeClient::InitializeFake() {
  InitializeFake(FakeCiceroneClient::Get());
}

// static
void ConciergeClient::InitializeFake(FakeCiceroneClient* fake_cicerone_client) {
  // Do not create a new fake if it was initialized early in a browser test to
  // allow the test to set its own client.
  if (!FakeConciergeClient::Get()) {
    new FakeConciergeClient(fake_cicerone_client);
  }
}

// static
void ConciergeClient::Shutdown() {
  delete g_instance;
}

// static
ConciergeClient* ConciergeClient::Get() {
  return g_instance;
}

}  // namespace ash
