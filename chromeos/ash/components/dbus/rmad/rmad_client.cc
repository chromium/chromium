// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/rmad/rmad_client.h"

#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/dbus/rmad/fake_rmad_client.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace {

RmadClient* g_instance = nullptr;

}  // namespace

class RmadClientImpl : public RmadClient {
 public:
  void Init(dbus::Bus* bus);

  bool WasRmaStateDetected() override;
  void SetRmaRequiredCallbackForSessionManager(
      base::OnceClosure session_manager_callback) override;
  void GetCurrentState(
      chromeos::DBusMethodCallback<rmad::GetStateReply> callback) override;
  void TransitionNextState(
      const rmad::RmadState& state,
      chromeos::DBusMethodCallback<rmad::GetStateReply> callback) override;
  void TransitionPreviousState(
      chromeos::DBusMethodCallback<rmad::GetStateReply> callback) override;
  void AbortRma(
      chromeos::DBusMethodCallback<rmad::AbortRmaReply> callback) override;
  void GetLog(
      chromeos::DBusMethodCallback<rmad::GetLogReply> callback) override;
  void SaveLog(
      const std::string& diagnostics_log_text,
      chromeos::DBusMethodCallback<rmad::SaveLogReply> callback) override;
  void RecordBrowserActionMetric(
      const rmad::RecordBrowserActionMetricRequest request,
      chromeos::DBusMethodCallback<rmad::RecordBrowserActionMetricReply>
          callback) override;
  void ExtractExternalDiagnosticsApp(
      chromeos::DBusMethodCallback<rmad::ExtractExternalDiagnosticsAppReply>
          callback) override;
  void InstallExtractedDiagnosticsApp(
      chromeos::DBusMethodCallback<rmad::InstallExtractedDiagnosticsAppReply>
          callback) override;
  void GetInstalledDiagnosticsApp(
      chromeos::DBusMethodCallback<rmad::GetInstalledDiagnosticsAppReply>
          callback) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;

  RmadClientImpl() = default;
  RmadClientImpl(const RmadClientImpl&) = delete;
  RmadClientImpl& operator=(const RmadClientImpl&) = delete;
  ~RmadClientImpl() override = default;

 private:
  void CheckIfRmaIsRequired();
  void OnCheckIfRmaIsRequired(dbus::Response* response);

  template <class T>
  void OnProtoReply(chromeos::DBusMethodCallback<T> callback,
                    dbus::Response* response);

  void CalibrationProgressReceived(dbus::Signal* signal);
  void CalibrationOverallProgressReceived(dbus::Signal* signal);
  void ErrorReceived(dbus::Signal* signal);
  void HardwareWriteProtectionStateReceived(dbus::Signal* signal);
  void PowerCableStateReceived(dbus::Signal* signal);
  void ExternalDiskStatusReceived(dbus::Signal* signal);
  void ProvisioningProgressReceived(dbus::Signal* signal);
  void HardwareVerificationResultReceived(dbus::Signal* signal);
  void FinalizationProgressReceived(dbus::Signal* signal);
  void RoFirmwareUpdateProgressReceived(dbus::Signal* signal);

  void SignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success);

  // Saves the response from base::PathExists().
  void OnFetchRmadExecutableExists(bool exists);

  // Saves the response from base::PathExists().
  void OnFetchRmadStateFileExists(bool exists);

  // Sends out the requests to verify if RMAD files exist on device.
  void StartCheckForRmadFiles();

  raw_ptr<dbus::ObjectProxy> rmad_proxy_ = nullptr;
  base::ObserverList<Observer, /*check_empty=*/true, /*allow_reentrancy=*/false>
      observers_;

  // True if the RMAD executable exists.
  std::optional<bool> rma_executable_exists_;

  // True if the RMAD state file exists.
  std::optional<bool> rma_state_file_exists_;

  // Set from the response to the RMA daemon D-Bus call.
  bool is_rma_required_ = false;

  // The ChromeSessionManager callback invoked when |is_rma_required_| is set.
  base::OnceClosure session_manager_callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RmadClientImpl> weak_ptr_factory_{this};
};

void RmadClientImpl::Init(dbus::Bus* bus) {
  StartCheckForRmadFiles();
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
      {rmad::kExternalDiskDetectedSignal,
       &RmadClientImpl::ExternalDiskStatusReceived},
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
  CheckIfRmaIsRequired();
}

void RmadClientImpl::CheckIfRmaIsRequired() {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName,
                               rmad::kIsRmaRequiredMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnCheckIfRmaIsRequired,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RmadClientImpl::OnCheckIfRmaIsRequired(dbus::Response* response) {
  // TODO(b/230924565): Remove LOG statement after feature release.
  VLOG(1) << "RmadClientImpl::OnCheckIfRmaIsRequired rma_executable_exists_: "
          << rma_executable_exists_.value_or(false)
          << " rma_state_file_exists_: "
          << rma_state_file_exists_.value_or(false);

  if (!response) {
    LOG(ERROR) << "Error calling rmad function for OnCheckIfRmaIsRequired";
    return;
  }

  dbus::MessageReader reader(response);
  if (!reader.PopBool(&is_rma_required_)) {
    LOG(ERROR) << "Unable to decode response for " << response->GetMember();
    return;
  }
  DCHECK(!reader.HasMoreData());

  if (!is_rma_required_) {
    // TODO(b/230924565): Remove LOG statement after feature release.
    VLOG(1) << "RmadClientImpl::OnCheckIfRmaIsRequired RMA is not required";

    // If RMA isn't required, callback is no longer needed.
    session_manager_callback_.Reset();
    return;
  }

  // TODO(b/230924565): Remove LOG statements after feature release.
  if (session_manager_callback_) {
    VLOG(1) << "RmadClientImpl::OnCheckIfRmaIsRequired invoking session "
               "manager callback";
    std::move(session_manager_callback_).Run();
  } else {
    VLOG(1) << "RmadClientImpl::OnCheckIfRmaIsRequired the callback is not set";
  }
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
  int32_t overall_progress_status;
  if (!reader.PopInt32(&overall_progress_status)) {
    LOG(ERROR) << "Unable to decode overall progress status int32 from "
               << signal->GetMember() << " signal";
    return;
  }
  DCHECK(!reader.HasMoreData());
  for (auto& observer : observers_) {
    observer.CalibrationOverallProgress(
        static_cast<rmad::CalibrationOverallStatus>(overall_progress_status));
  }
}

void RmadClientImpl::ErrorReceived(dbus::Signal* signal) {
  DCHECK_EQ(signal->GetMember(), rmad::kErrorSignal);
  dbus::MessageReader reader(signal);
  int32_t error;
  if (!reader.PopInt32(&error)) {
    LOG(ERROR) << "Unable to decode error int32 from " << signal->GetMember()
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

void RmadClientImpl::ExternalDiskStatusReceived(dbus::Signal* signal) {
  DCHECK_EQ(signal->GetMember(), rmad::kExternalDiskDetectedSignal);
  dbus::MessageReader reader(signal);
  bool detected;
  if (!reader.PopBool(&detected)) {
    LOG(ERROR) << "Unable to decode detected bool from " << signal->GetMember()
               << " signal";
    return;
  }
  DCHECK(!reader.HasMoreData());
  for (auto& observer : observers_) {
    observer.ExternalDiskState(detected);
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
  int32_t error;
  if (!sub_reader.PopInt32(&status) || !sub_reader.PopDouble(&progress) ||
      !sub_reader.PopInt32(&error)) {
    LOG(ERROR) << "Unable to decode signal for " << signal->GetMember();
    return;
  }
  rmad::ProvisionStatus signal_proto;
  signal_proto.set_status(static_cast<rmad::ProvisionStatus::Status>(status));
  signal_proto.set_progress(progress);
  signal_proto.set_error(static_cast<rmad::ProvisionStatus::Error>(error));
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
  int32_t error;
  if (!sub_reader.PopInt32(&status) || !sub_reader.PopDouble(&progress) ||
      !sub_reader.PopInt32(&error)) {
    LOG(ERROR) << "Unable to decode signal for " << signal->GetMember();
    return;
  }
  rmad::FinalizeStatus signal_proto;
  signal_proto.set_status(static_cast<rmad::FinalizeStatus::Status>(status));
  signal_proto.set_progress(progress);
  signal_proto.set_error(static_cast<rmad::FinalizeStatus::Error>(error));
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
    chromeos::DBusMethodCallback<rmad::GetStateReply> callback) {
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
    chromeos::DBusMethodCallback<rmad::GetStateReply> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName,
                               rmad::kTransitionNextStateMethod);
  dbus::MessageWriter writer(&method_call);
  // Create the empty request proto.
  rmad::TransitionNextStateRequest protobuf_request;
  protobuf_request.set_allocated_state(new rmad::RmadState(state));
  if (!writer.AppendProtoAsArrayOfBytes(protobuf_request)) {
    LOG(ERROR) << "Error constructing message for "
               << rmad::kTransitionNextStateMethod;
    std::move(callback).Run(std::nullopt);
    return;
  }
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<rmad::GetStateReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}
void RmadClientImpl::TransitionPreviousState(
    chromeos::DBusMethodCallback<rmad::GetStateReply> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName,
                               rmad::kTransitionPreviousStateMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<rmad::GetStateReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RmadClientImpl::AbortRma(
    chromeos::DBusMethodCallback<rmad::AbortRmaReply> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName, rmad::kAbortRmaMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<rmad::AbortRmaReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RmadClientImpl::GetLog(
    chromeos::DBusMethodCallback<rmad::GetLogReply> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName, rmad::kGetLogMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<rmad::GetLogReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RmadClientImpl::SaveLog(
    const std::string& diagnostics_log_text,
    chromeos::DBusMethodCallback<rmad::SaveLogReply> callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName, rmad::kSaveLogMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(diagnostics_log_text);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<rmad::SaveLogReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RmadClientImpl::RecordBrowserActionMetric(
    const rmad::RecordBrowserActionMetricRequest request,
    chromeos::DBusMethodCallback<rmad::RecordBrowserActionMetricReply>
        callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName,
                               rmad::kRecordBrowserActionMetricMethod);
  dbus::MessageWriter writer(&method_call);

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    LOG(ERROR) << "Error constructing message for "
               << rmad::kRecordBrowserActionMetricMethod;
    std::move(callback).Run(std::nullopt);
    return;
  }

  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(
          &RmadClientImpl::OnProtoReply<rmad::RecordBrowserActionMetricReply>,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RmadClientImpl::ExtractExternalDiagnosticsApp(
    chromeos::DBusMethodCallback<rmad::ExtractExternalDiagnosticsAppReply>
        callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName,
                               rmad::kExtractExternalDiagnosticsAppMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<
                         rmad::ExtractExternalDiagnosticsAppReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RmadClientImpl::InstallExtractedDiagnosticsApp(
    chromeos::DBusMethodCallback<rmad::InstallExtractedDiagnosticsAppReply>
        callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName,
                               rmad::kInstallExtractedDiagnosticsAppMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&RmadClientImpl::OnProtoReply<
                         rmad::InstallExtractedDiagnosticsAppReply>,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void RmadClientImpl::GetInstalledDiagnosticsApp(
    chromeos::DBusMethodCallback<rmad::GetInstalledDiagnosticsAppReply>
        callback) {
  dbus::MethodCall method_call(rmad::kRmadInterfaceName,
                               rmad::kGetInstalledDiagnosticsAppMethod);
  dbus::MessageWriter writer(&method_call);
  rmad_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(
          &RmadClientImpl::OnProtoReply<rmad::GetInstalledDiagnosticsAppReply>,
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
void RmadClientImpl::OnProtoReply(chromeos::DBusMethodCallback<T> callback,
                                  dbus::Response* response) {
  if (!response) {
    LOG(ERROR) << "Error calling rmad function";
    std::move(callback).Run(std::nullopt);
    return;
  }

  dbus::MessageReader reader(response);
  T response_proto;
  if (!reader.PopArrayOfBytesAsProto(&response_proto)) {
    LOG(ERROR) << "Unable to decode response for " << response->GetMember();
    std::move(callback).Run(std::nullopt);
    return;
  }
  DCHECK(!reader.HasMoreData());

  std::move(callback).Run(std::move(response_proto));
}

void RmadClientImpl::StartCheckForRmadFiles() {
  base::FilePath rmad_executable_path;
  if (!base::PathService::Get(
          chromeos::dbus_paths::FILE_RMAD_SERVICE_EXECUTABLE,
          &rmad_executable_path)) {
    LOG(ERROR)
        << "Could not get rmad executable path. RMA will not be available";
    return;
  }
  base::FilePath rmad_state_file_path;
  if (!base::PathService::Get(chromeos::dbus_paths::FILE_RMAD_SERVICE_STATE,
                              &rmad_state_file_path)) {
    LOG(ERROR)
        << "Could not get rmad state file path. RMA will not be available";
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&base::PathExists, rmad_executable_path),
      base::BindOnce(&RmadClientImpl::OnFetchRmadExecutableExists,
                     weak_ptr_factory_.GetWeakPtr()));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&base::PathExists, rmad_state_file_path),
      base::BindOnce(&RmadClientImpl::OnFetchRmadStateFileExists,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RmadClientImpl::OnFetchRmadExecutableExists(bool exists) {
  rma_executable_exists_ = exists;
}

void RmadClientImpl::OnFetchRmadStateFileExists(bool exists) {
  rma_state_file_exists_ = exists;
}

bool RmadClientImpl::WasRmaStateDetected() {
  if (!rma_executable_exists_.has_value()) {
    LOG(WARNING) << "Checking if RMA executable exists not completed before "
                    "WasRmaStateDetected() called.";
  }
  if (!rma_state_file_exists_.has_value()) {
    LOG(WARNING) << "Checking if RMA state file exists not completed before "
                    "WasRmaStateDetected() called.";
  }

  // TODO(b/230924565): Remove LOG statement after feature release.
  VLOG(1) << "RmadClientImpl::WasRmaStateDetected rma_executable_exists_: "
          << rma_executable_exists_.value_or(false)
          << " is_rma_required_: " << is_rma_required_
          << " rma_state_file_exists_: "
          << rma_state_file_exists_.value_or(false);

  return rma_executable_exists_.value_or(false) &&
         (is_rma_required_ || rma_state_file_exists_.value_or(false));
}

void RmadClientImpl::SetRmaRequiredCallbackForSessionManager(
    base::OnceClosure session_manager_callback) {
  if (WasRmaStateDetected()) {
    std::move(session_manager_callback).Run();
  } else {
    session_manager_callback_ = std::move(session_manager_callback);
  }
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
void RmadClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
RmadClient* RmadClient::Get() {
  return g_instance;
}

// static
void RmadClient::InitializeFake() {
  // Do not create a new fake if it was initialized early in a browser test (for
  // early setup calls dependent on RmadClient).
  if (!FakeRmadClient::Get()) {
    new FakeRmadClient();
  }
}

}  // namespace ash
