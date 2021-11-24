// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/rmad/rmad_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "chromeos/dbus/rmad/fake_rmad_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace {
RmadClient* g_instance = nullptr;
}  // namespace

class RmadClientImpl : public RmadClient {
 public:
  void Init(dbus::Bus* bus);

  void CheckInRma(DBusMethodCallback<bool> callback) override;

  void GetCurrentState(
      DBusMethodCallback<rmad::GetStateReply> callback) override;
  void TransitionNextState(
      const rmad::RmadState& state,
      DBusMethodCallback<rmad::GetStateReply> callback) override;
  void TransitionPreviousState(
      DBusMethodCallback<rmad::GetStateReply> callback) override;

  void AbortRma(DBusMethodCallback<rmad::AbortRmaReply> callback) override;

  void GetLog(DBusMethodCallback<std::string> callback) override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;

  RmadClientImpl() = default;
  RmadClientImpl(const RmadClientImpl&) = delete;
  RmadClientImpl& operator=(const RmadClientImpl&) = delete;
  ~RmadClientImpl() override = default;

 private:
  void OnCheckInRma(DBusMethodCallback<bool> callback,
                    dbus::Response* response);

  template <class T>
  void OnProtoReply(DBusMethodCallback<T> callback, dbus::Response* response);

  void OnGetLogReply(DBusMethodCallback<std::string> callback,
                     dbus::Response* response);

  void CalibrationProgressReceived(dbus::Signal* signal);
  void CalibrationOverallProgressReceived(dbus::Signal* signal);
  void ErrorReceived(dbus::Signal* signal);
  void HardwareWriteProtectionStateReceived(dbus::Signal* signal);
  void PowerCableStateReceived(dbus::Signal* signal);
  void ProvisioningProgressReceived(dbus::Signal* signal);
  void HardwareVerificationResultReceived(dbus::Signal* signal);
  void FinalizationProgressReceived(dbus::Signal* signal);
  void RoFirmwareUpdateProgressReceived(dbus::Signal* signal);

  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success);

  dbus::ObjectProxy* rmad_proxy_ = nullptr;
  base::ObserverList<Observer, /*check_empty=*/true, /*allow_reentrancy=*/false>
      observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RmadClientImpl> weak_ptr_factory_{this};
};

void RmadClientImpl::Init(dbus::Bus* bus) {
  rmad_proxy_ = bus->GetObjectProxy(rmad::kRmadServiceName,
                                    dbus::ObjectPath(rmad::kRmadServicePath));
  // Listen to D-Bus signals emitted by rmad.
  typedef void (RmadClientImpl::*SignalMethod)(dbus::Signal*);
  const std::pair<const char*, SignalMethod> kSignalMethods[] = {
      {rmad::kCalibrationProgressSignal,
       &RmadClientImpl::CalibrationProgressReceived},
      {rmad::kCalibrationOverallSignal,
       &RmadClientImpl::CalibrationOverallProgressReceived},
      {rmad::kErrorSignal, &RmadClientImpl::ErrorReceived},
      {rmad::kHardwareWriteProtectionStateSignal,
       &RmadClientImpl::HardwareWriteProtectionStateReceived},
      {rmad::kPowerCableStateSignal, &RmadClientImpl::PowerCableStateReceived},
      {rmad::kProvisioningProgressSignal,
       &RmadClientImpl::ProvisioningProgressReceived},
      {rmad::kHardwareVerificationResultSignal,
       &RmadClientImpl::HardwareVerificationResultReceived},
      {rmad::kFinalizeProgressSignal,
       &RmadClientImpl::FinalizationProgressReceived},
      {rmad::kUpdateRoFirmwareStatusSignal,
       &RmadClientImpl::RoFirmwareUpdateProgressReceived},
  };
  auto on_connected_callback = base::BindRepeating(
      &RmadClientImpl::SignalConnected, weak_ptr_factory_.GetWeakPtr());
  for (const auto& p : kSignalMethods) {
    rmad_proxy_->ConnectToSignal(
        rmad::kRmadInterfaceName, p.first,
        base::BindRepeating(p.second, weak_ptr_factory_.GetWeakPtr()),
        on_connected_callback);
  }
}

void RmadClientImpl::CheckInRma(DBusMethodCallback<bool> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName,
                               rmad::kIsRmaRequiredMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnCheckInRma,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RmadClientImpl::OnCheckInRma(DBusMethodCallback<bool> callback,
                                  dbus::Response* response) {
  if (!response) {
    LOG(ERROR) << "Error calling rmad function for OnCheckInRma";
    std::move(callback).Run(false);
    return;
  }

  dbus::MessageReader reader(response);
  bool is_rma_required = false;
  if (!reader.PopBool(&is_rma_required)) {
    LOG(ERROR) << "Unable to decode response for " << response->GetMember();
    std::move(callback).Run(false);
    return;
  }
  DCHECK(!reader.HasMoreData());
  std::move(callback).Run(is_rma_required);
}

// Called when a dbus signal is initially connected.
void RmadClientImpl::SignalConnected(const std::string& interface_name,
                                     const std::string& signal_name,
                                     bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to connect to signal " << signal_name << ".";
  }
}

void RmadClientImpl::CalibrationProgressReceived(dbus::Signal* signal) {
  DCHECK_EQ(signal->GetMember(), rmad::kCalibrationProgressSignal);
  dbus::MessageReader reader(signal);
  // Read proto message
  dbus::MessageReader sub_reader(nullptr);
  if (!reader.PopStruct(&sub_reader)) {
    LOG(ERROR) << "Unable to decode signal for " << signal->GetMember();
    return;
  }
  DCHECK(!reader.HasMoreData());
  int32_t component;
  int32_t status;
  double progress;
  if (!sub_reader.PopInt32(&component) || !sub_reader.PopInt32(&status) ||
      !sub_reader.PopDouble(&progress)) {
    LOG(ERROR) << "Unable to decode signal for " << signal->GetMember();
    return;
  }
  rmad::CalibrationComponentStatus signal_proto;
  signal_proto.set_component(static_cast<rmad::RmadComponent>(component));
  signal_proto.set_status(
      static_cast<rmad::CalibrationComponentStatus::CalibrationStatus>(status));
  signal_proto.set_progress(progress);
  for (auto& observer : observers_) {
    observer.CalibrationProgress(signal_proto);
  }
}

void RmadClientImpl::CalibrationOverallProgressReceived(dbus::Signal* signal) {
  DCHECK_EQ(signal->GetMember(), rmad::kCalibrationOverallSignal);
  dbus::MessageReader reader(signal);
  uint32_t overall_progress;
  if (!reader.PopUint32(&overall_progress)) {
    LOG(ERROR) << "Unable to decode overall progress uint32 from "
               << signal->GetMember() << " signal";
    return;
  }
  DCHECK(!reader.HasMoreData());
  for (auto& observer : observers_) {
    observer.CalibrationOverallProgress(
        static_cast<rmad::CalibrationOverallStatus>(overall_progress));
  }
}

void RmadClientImpl::ErrorReceived(dbus::Signal* signal) {
  DCHECK_EQ(signal->GetMember(), rmad::kErrorSignal);
  dbus::MessageReader reader(signal);
  uint32_t error;
  if (!reader.PopUint32(&error)) {
    LOG(ERROR) << "Unable to decode error uint32 from " << signal->GetMember()
               << " signal";
    return;
  }
  DCHECK(!reader.HasMoreData());
  for (auto& observer : observers_) {
    observer.Error(static_cast<rmad::RmadErrorCode>(error));
  }
}

void RmadClientImpl::HardwareWriteProtectionStateReceived(
    dbus::Signal* signal) {
  DCHECK_EQ(signal->GetMember(), rmad::kHardwareWriteProtectionStateSignal);
  dbus::MessageReader reader(signal);
  bool enabled;
  if (!reader.PopBool(&enabled)) {
    LOG(ERROR) << "Unable to decode enabled bool from " << signal->GetMember()
               << " signal";
    return;
  }
  DCHECK(!reader.HasMoreData());
  for (auto& observer : observers_) {
    observer.HardwareWriteProtectionState(enabled);
  }
}

void RmadClientImpl::PowerCableStateReceived(dbus::Signal* signal) {
  DCHECK_EQ(signal->GetMember(), rmad::kPowerCableStateSignal);
  dbus::MessageReader reader(signal);
  bool plugged_in;
  if (!reader.PopBool(&plugged_in)) {
    LOG(ERROR) << "Unable to decode plugged_in bool from "
               << signal->GetMember() << " signal";
    return;
  }
  DCHECK(!reader.HasMoreData());
  for (auto& observer : observers_) {
    observer.PowerCableState(plugged_in);
  }
}

void RmadClientImpl::ProvisioningProgressReceived(dbus::Signal* signal) {
  DCHECK_EQ(signal->GetMember(), rmad::kProvisioningProgressSignal);
  dbus::MessageReader reader(signal);
  // Read proto message
  dbus::MessageReader sub_reader(nullptr);
  if (!reader.PopStruct(&sub_reader)) {
    LOG(ERROR) << "Unable to decode signal for " << signal->GetMember();
    return;
  }
  DCHECK(!reader.HasMoreData());
  int32_t status;
  double progress;
  if (!sub_reader.PopInt32(&status) || !sub_reader.PopDouble(&progress)) {
    LOG(ERROR) << "Unable to decode signal for " << signal->GetMember();
    return;
  }
  rmad::ProvisionStatus signal_proto;
  signal_proto.set_status(static_cast<rmad::ProvisionStatus::Status>(status));
  signal_proto.set_progress(progress);
  for (auto& observer : observers_) {
    observer.ProvisioningProgress(signal_proto);
  }
}

void RmadClientImpl::HardwareVerificationResultReceived(dbus::Signal* signal) {
  DCHECK_EQ(signal->GetMember(), rmad::kHardwareVerificationResultSignal);
  dbus::MessageReader reader(signal);
  // Read message
  dbus::MessageReader sub_reader(nullptr);
  if (!reader.PopStruct(&sub_reader)) {
    LOG(ERROR) << "Unable to decode signal for " << signal->GetMember();
    return;
  }
  DCHECK(!reader.HasMoreData());
  bool is_compliant = true;
  std::string error_str = "";
  if (!sub_reader.PopBool(&is_compliant) || !sub_reader.PopString(&error_str)) {
    LOG(ERROR) << "Unable to decode signal for " << signal->GetMember();
    return;
  }
  DCHECK(!reader.HasMoreData());
  rmad::HardwareVerificationResult signal_proto;
  signal_proto.set_is_compliant(is_compliant);
  signal_proto.set_error_str(error_str);
  for (auto& observer : observers_) {
    observer.HardwareVerificationResult(signal_proto);
  }
}

void RmadClientImpl::FinalizationProgressReceived(dbus::Signal* signal) {
  DCHECK_EQ(signal->GetMember(), rmad::kFinalizeProgressSignal);
  dbus::MessageReader reader(signal);
  // Read message
  dbus::MessageReader sub_reader(nullptr);
  if (!reader.PopStruct(&sub_reader)) {
    LOG(ERROR) << "Unable to decode signal for " << signal->GetMember();
    return;
  }
  DCHECK(!reader.HasMoreData());
  int32_t status;
  double progress;
  if (!sub_reader.PopInt32(&status) || !sub_reader.PopDouble(&progress)) {
    LOG(ERROR) << "Unable to decode signal for " << signal->GetMember();
    return;
  }
  rmad::FinalizeStatus signal_proto;
  signal_proto.set_status(static_cast<rmad::FinalizeStatus::Status>(status));
  signal_proto.set_progress(progress);
  for (auto& observer : observers_) {
    observer.FinalizationProgress(signal_proto);
  }
}

void RmadClientImpl::RoFirmwareUpdateProgressReceived(dbus::Signal* signal) {
  DCHECK_EQ(signal->GetMember(), rmad::kUpdateRoFirmwareStatusSignal);
  dbus::MessageReader reader(signal);
  // Read message
  int32_t status;
  if (!reader.PopInt32(&status)) {
    LOG(ERROR) << "Unable to decode signal for " << signal->GetMember();
    return;
  }
  DCHECK(!reader.HasMoreData());
  for (auto& observer : observers_) {
    observer.RoFirmwareUpdateProgress(
        static_cast<rmad::UpdateRoFirmwareStatus>(status));
  }
}

void RmadClientImpl::GetCurrentState(
    DBusMethodCallback<rmad::GetStateReply> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName,
                               rmad::kGetCurrentStateMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<rmad::GetStateReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RmadClientImpl::TransitionNextState(
    const rmad::RmadState& state,
    DBusMethodCallback<rmad::GetStateReply> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName,
                               rmad::kTransitionNextStateMethod);
  dbus::MessageWriter writer(&method_call);
  // Create the empty request proto.
  rmad::TransitionNextStateRequest protobuf_request;
  protobuf_request.set_allocated_state(new rmad::RmadState(state));
  if (!writer.AppendProtoAsArrayOfBytes(protobuf_request)) {
    LOG(ERROR) << "Error constructing message for "
               << rmad::kTransitionNextStateMethod;
    std::move(callback).Run(absl::nullopt);
    return;
  }
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<rmad::GetStateReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}
void RmadClientImpl::TransitionPreviousState(
    DBusMethodCallback<rmad::GetStateReply> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName,
                               rmad::kTransitionPreviousStateMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<rmad::GetStateReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RmadClientImpl::AbortRma(
    DBusMethodCallback<rmad::AbortRmaReply> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName, rmad::kAbortRmaMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<rmad::AbortRmaReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RmadClientImpl::GetLog(DBusMethodCallback<std::string> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName, rmad::kGetLogMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnGetLogReply,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RmadClientImpl::AddObserver(Observer* observer) {
  // Currently there is only one observer (chromeos::ShimlessRmaService) and it
  // is added before any signals are expected, so there is no need to preserve
  // any signals and send them to new observers.
  CHECK(observer);
  observers_.AddObserver(observer);
}

void RmadClientImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool RmadClientImpl::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

template <class T>
void RmadClientImpl::OnProtoReply(DBusMethodCallback<T> callback,
                                  dbus::Response* response) {
  if (!response) {
    LOG(ERROR) << "Error calling rmad function";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  dbus::MessageReader reader(response);
  T response_proto;
  if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
    LOG(ERROR) << "Unable to decode response for " << response->GetMember();
    std::move(callback).Run(absl::nullopt);
    return;
  }
  DCHECK(!reader.HasMoreData());

  std::move(callback).Run(std::move(response_proto));
}

void RmadClientImpl::OnGetLogReply(DBusMethodCallback<std::string> callback,
                                   dbus::Response* response) {
  if (!response) {
    LOG(ERROR) << "Error calling rmad function";
    std::move(callback).Run(absl::nullopt);
    return;
  }

  dbus::MessageReader reader(response);
  std::string log_path;
  if (!reader.PopString(&log_path)) {
    LOG(ERROR) << "Unable to read string for " << response->GetMember();
    std::move(callback).Run(absl::nullopt);
    return;
  }
  DCHECK(!reader.HasMoreData());

  std::move(callback).Run(std::move(log_path));
}

RmadClient::RmadClient() {
  CHECK(!g_instance);
  g_instance = this;
}

RmadClient::~RmadClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void RmadClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new RmadClientImpl())->Init(bus);
}

// static
void RmadClient::InitializeFake() {
  FakeRmadClient::CreateWithState();
}

// static
void RmadClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
RmadClient* RmadClient::Get() {
  return g_instance;
}

}  // namespace chromeos
