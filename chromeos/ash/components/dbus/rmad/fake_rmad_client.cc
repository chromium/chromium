// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/rmad/fake_rmad_client.h"

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {
namespace {

constexpr char rsu_challenge_code[] =
    "HRBXHV84NSTHT25WJECYQKB8SARWFTMSWNGFT2FVEEPX69VE99USV3QFBEANDVXGQVL93QK2M6"
    "P3DNV4";
constexpr char rsu_hwid[] = "SAMUSTEST_2082";
constexpr char rsu_challenge_url[] =
    "https://chromeos.google.com/partner/console/"
    "cr50reset?challenge="
    "HRBXHV84NSTHT25WJECYQKB8SARWFTMSWNGFT2FVEEPX69VE99USV3QFBEANDVXGQVL93QK2M6"
    "P3DNV4&hwid=SAMUSTEST_2082";

rmad::RmadState* CreateState(rmad::RmadState::StateCase state_case) {
  rmad::RmadState* state = new rmad::RmadState();
  switch (state_case) {
    case rmad::RmadState::kWelcome:
      state->set_allocated_welcome(new rmad::WelcomeState());
      break;
    case rmad::RmadState::kComponentsRepair:
      state->set_allocated_components_repair(new rmad::ComponentsRepairState());
      break;
    case rmad::RmadState::kDeviceDestination:
      state->set_allocated_device_destination(
          new rmad::DeviceDestinationState());
      break;
    case rmad::RmadState::kWpDisableMethod:
      state->set_allocated_wp_disable_method(
          new rmad::WriteProtectDisableMethodState());
      break;
    case rmad::RmadState::kWpDisableRsu:
      state->set_allocated_wp_disable_rsu(
          new rmad::WriteProtectDisableRsuState());
      break;
    case rmad::RmadState::kWpDisablePhysical:
      state->set_allocated_wp_disable_physical(
          new rmad::WriteProtectDisablePhysicalState());
      break;
    case rmad::RmadState::kWpDisableComplete:
      state->set_allocated_wp_disable_complete(
          new rmad::WriteProtectDisableCompleteState());
      break;
    case rmad::RmadState::kUpdateRoFirmware:
      state->set_allocated_update_ro_firmware(
          new rmad::UpdateRoFirmwareState());
      break;
    case rmad::RmadState::kRestock:
      state->set_allocated_restock(new rmad::RestockState());
      break;
    case rmad::RmadState::kUpdateDeviceInfo:
      state->set_allocated_update_device_info(
          new rmad::UpdateDeviceInfoState());
      break;
    case rmad::RmadState::kCheckCalibration:
      state->set_allocated_check_calibration(new rmad::CheckCalibrationState());
      break;
    case rmad::RmadState::kSetupCalibration:
      state->set_allocated_setup_calibration(new rmad::SetupCalibrationState());
      break;
    case rmad::RmadState::kRunCalibration:
      state->set_allocated_run_calibration(new rmad::RunCalibrationState());
      break;
    case rmad::RmadState::kProvisionDevice:
      state->set_allocated_provision_device(new rmad::ProvisionDeviceState());
      break;
    case rmad::RmadState::kWpEnablePhysical:
      state->set_allocated_wp_enable_physical(
          new rmad::WriteProtectEnablePhysicalState());
      break;
    case rmad::RmadState::kFinalize:
      state->set_allocated_finalize(new rmad::FinalizeState());
      break;
    case rmad::RmadState::kRepairComplete:
      state->set_allocated_repair_complete(new rmad::RepairCompleteState());
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return state;
}

rmad::GetStateReply CreateStateReply(rmad::RmadState::StateCase state,
                                     rmad::RmadErrorCode error,
                                     bool can_go_back = true,
                                     bool can_abort = true) {
  rmad::GetStateReply reply;
  reply.set_allocated_state(CreateState(state));
  reply.set_error(error);
  reply.set_can_go_back(can_go_back);
  reply.set_can_abort(can_abort);
  return reply;
}
}  // namespace

FakeRmadClient::FakeRmadClient() {
  // Default to abortable.
  SetAbortable(true);
}

FakeRmadClient::~FakeRmadClient() = default;

// static
FakeRmadClient* FakeRmadClient::Get() {
  RmadClient* client = RmadClient::Get();
  return static_cast<FakeRmadClient*>(client);
}

void FakeRmadClient::GetCurrentState(
    chromeos::DBusMethodCallback<rmad::GetStateReply> callback) {
  if (NumStates() > 0) {
    CHECK(state_index_ < NumStates());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), GetStateReply()));
  } else {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
  }
  TriggerHardwareVerificationResultObservation(true, "");
}

void FakeRmadClient::TransitionNextState(
    const rmad::RmadState& state,
    chromeos::DBusMethodCallback<rmad::GetStateReply> callback) {
  if (NumStates() == 0) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
    return;
  }
  CHECK_LT(state_index_, NumStates());
  if (state.state_case() != GetStateCase()) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_REQUEST_INVALID);
    reply.set_allocated_state(new rmad::RmadState(GetState()));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
    return;
  }
  if (state_index_ >= NumStates() - 1) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_TRANSITION_FAILED);
    reply.set_allocated_state(new rmad::RmadState(GetState()));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
    return;
  }
  // Update the fake state with the new data.
  if (state_index_ < NumStates()) {
    // TODO(gavindodd): Maybe the state should not update if the existing state
    // has an error?
    state_replies_[state_index_].set_allocated_state(
        new rmad::RmadState(state));
  }

  state_index_++;
  CHECK_LT(state_index_, NumStates());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GetStateReply()));
}

void FakeRmadClient::TransitionPreviousState(
    chromeos::DBusMethodCallback<rmad::GetStateReply> callback) {
  if (NumStates() == 0) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
    return;
  }
  CHECK_LT(state_index_, NumStates());
  if (state_index_ == 0) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_TRANSITION_FAILED);
    reply.set_allocated_state(new rmad::RmadState(GetState()));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
    return;
  }
  state_index_--;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GetStateReply()));
}

void FakeRmadClient::AbortRma(
    chromeos::DBusMethodCallback<rmad::AbortRmaReply> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     std::optional<rmad::AbortRmaReply>(abort_rma_reply_)));
}

void FakeRmadClient::GetLog(
    chromeos::DBusMethodCallback<rmad::GetLogReply> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     std::optional<rmad::GetLogReply>(get_log_reply_)));
}

void FakeRmadClient::SaveLog(
    const std::string& diagnostics_log_text,
    chromeos::DBusMethodCallback<rmad::SaveLogReply> callback) {
  diagnostics_logs_text_ = diagnostics_log_text;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     std::optional<rmad::SaveLogReply>(save_log_reply_)));
}

void FakeRmadClient::RecordBrowserActionMetric(
    const rmad::RecordBrowserActionMetricRequest request,
    chromeos::DBusMethodCallback<rmad::RecordBrowserActionMetricReply>
        callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     std::optional<rmad::RecordBrowserActionMetricReply>(
                         record_browser_action_metric_reply_)));
}

void FakeRmadClient::ExtractExternalDiagnosticsApp(
    chromeos::DBusMethodCallback<rmad::ExtractExternalDiagnosticsAppReply>
        callback) {
  rmad::ExtractExternalDiagnosticsAppReply reply;
  if (external_diag_app_path_.empty()) {
    reply.set_error(rmad::RMAD_ERROR_DIAGNOSTICS_APP_NOT_FOUND);
  } else {
    reply.set_error(rmad::RMAD_ERROR_OK);
    reply.set_diagnostics_app_swbn_path(
        external_diag_app_path_.AddExtension("swbn").value());
    reply.set_diagnostics_app_crx_path(
        external_diag_app_path_.AddExtension("crx").value());
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          std::optional<rmad::ExtractExternalDiagnosticsAppReply>(reply)));
}

void FakeRmadClient::InstallExtractedDiagnosticsApp(
    chromeos::DBusMethodCallback<rmad::InstallExtractedDiagnosticsAppReply>
        callback) {
  rmad::InstallExtractedDiagnosticsAppReply reply;
  if (external_diag_app_path_.empty()) {
    reply.set_error(rmad::RMAD_ERROR_DIAGNOSTICS_APP_NOT_FOUND);
  } else {
    installed_diag_app_path_ = external_diag_app_path_;
    reply.set_error(rmad::RMAD_ERROR_OK);
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          std::optional<rmad::InstallExtractedDiagnosticsAppReply>(reply)));
}

void FakeRmadClient::GetInstalledDiagnosticsApp(
    chromeos::DBusMethodCallback<rmad::GetInstalledDiagnosticsAppReply>
        callback) {
  rmad::GetInstalledDiagnosticsAppReply reply;
  if (installed_diag_app_path_.empty()) {
    reply.set_error(rmad::RMAD_ERROR_DIAGNOSTICS_APP_NOT_FOUND);
  } else {
    reply.set_error(rmad::RMAD_ERROR_OK);
    reply.set_diagnostics_app_swbn_path(
        installed_diag_app_path_.AddExtension("swbn").value());
    reply.set_diagnostics_app_crx_path(
        installed_diag_app_path_.AddExtension("crx").value());
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback),
          std::optional<rmad::GetInstalledDiagnosticsAppReply>(reply)));
}

void FakeRmadClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeRmadClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeRmadClient::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void FakeRmadClient::SetFakeStates() {
  // Set up fake component repair state.
  rmad::GetStateReply components_repair_state =
      CreateStateReply(rmad::RmadState::kComponentsRepair, rmad::RMAD_ERROR_OK);
  rmad::ComponentsRepairState::ComponentRepairStatus* component =
      components_repair_state.mutable_state()
          ->mutable_components_repair()
          ->add_components();
  component->set_component(rmad::RmadComponent::RMAD_COMPONENT_CAMERA);
  component->set_repair_status(
      rmad::ComponentsRepairState::ComponentRepairStatus::
          RMAD_REPAIR_STATUS_UNKNOWN);
  // Set up fake disable RSU state.
  rmad::GetStateReply wp_disable_rsu_state =
      CreateStateReply(rmad::RmadState::kWpDisableRsu, rmad::RMAD_ERROR_OK);
  wp_disable_rsu_state.mutable_state()
      ->mutable_wp_disable_rsu()
      ->set_allocated_challenge_code(new std::string(rsu_challenge_code));
  wp_disable_rsu_state.mutable_state()
      ->mutable_wp_disable_rsu()
      ->set_allocated_hwid(new std::string(rsu_hwid));
  wp_disable_rsu_state.mutable_state()
      ->mutable_wp_disable_rsu()
      ->set_allocated_challenge_url(new std::string(rsu_challenge_url));
  rmad::GetStateReply update_device_info =
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK);
  update_device_info.mutable_state()
      ->mutable_update_device_info()
      ->add_region_list("EMEA");
  update_device_info.mutable_state()
      ->mutable_update_device_info()
      ->add_region_list("APAC");
  update_device_info.mutable_state()
      ->mutable_update_device_info()
      ->add_region_list("AMER");
  update_device_info.mutable_state()
      ->mutable_update_device_info()
      ->add_sku_list(1UL);
  update_device_info.mutable_state()
      ->mutable_update_device_info()
      ->add_sku_list(2UL);
  update_device_info.mutable_state()
      ->mutable_update_device_info()
      ->add_sku_list(3UL);
  update_device_info.mutable_state()
      ->mutable_update_device_info()
      ->add_whitelabel_list("White-label 1");
  update_device_info.mutable_state()
      ->mutable_update_device_info()
      ->add_whitelabel_list("White-label 2");
  update_device_info.mutable_state()
      ->mutable_update_device_info()
      ->add_whitelabel_list("White-label 3");
  update_device_info.mutable_state()
      ->mutable_update_device_info()
      ->set_original_serial_number("serial 0001");
  update_device_info.mutable_state()
      ->mutable_update_device_info()
      ->set_original_region_index(2);
  update_device_info.mutable_state()
      ->mutable_update_device_info()
      ->set_original_sku_index(1);
  update_device_info.mutable_state()
      ->mutable_update_device_info()
      ->set_original_whitelabel_index(0);

  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      components_repair_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWpDisableMethod, rmad::RMAD_ERROR_OK),
      wp_disable_rsu_state,
      CreateStateReply(rmad::RmadState::kWpDisablePhysical,
                       rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWpDisableComplete,
                       rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kUpdateRoFirmware, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kRestock, rmad::RMAD_ERROR_OK),
      update_device_info,
      CreateStateReply(rmad::RmadState::kCheckCalibration, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kSetupCalibration, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kRunCalibration, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kProvisionDevice, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWpEnablePhysical, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kFinalize, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kRepairComplete, rmad::RMAD_ERROR_OK),
  };
  SetFakeStateReplies(fake_states);
  SetAbortable(true);
}

void FakeRmadClient::SetFakeStateReplies(
    std::vector<rmad::GetStateReply> fake_states) {
  state_replies_ = std::move(fake_states);
  state_index_ = 0;
}

bool FakeRmadClient::WasRmaStateDetected() {
  return NumStates() > 0;
}

void FakeRmadClient::SetRmaRequiredCallbackForSessionManager(
    base::OnceClosure session_manager_callback) {
  if (NumStates() > 0) {
    std::move(session_manager_callback).Run();
  }
}

void FakeRmadClient::SetAbortable(bool abortable) {
  // Abort RMA returns 'not in RMA' on success.
  abort_rma_reply_.set_error(abortable ? rmad::RMAD_ERROR_RMA_NOT_REQUIRED
                                       : rmad::RMAD_ERROR_CANNOT_CANCEL_RMA);
}

void FakeRmadClient::SetGetLogReply(const std::string& log,
                                    rmad::RmadErrorCode error) {
  get_log_reply_.set_log(log);
  get_log_reply_.set_error(error);
}

void FakeRmadClient::SetSaveLogReply(const std::string& save_path,
                                     rmad::RmadErrorCode error) {
  save_log_reply_.set_save_path(save_path);
  save_log_reply_.set_error(error);
}

void FakeRmadClient::SetRecordBrowserActionMetricReply(
    rmad::RmadErrorCode error) {
  record_browser_action_metric_reply_.set_error(error);
}

std::string FakeRmadClient::GetDiagnosticsLogsText() const {
  return diagnostics_logs_text_;
}

void FakeRmadClient::TriggerErrorObservation(rmad::RmadErrorCode error) {
  for (auto& observer : observers_) {
    observer.Error(error);
  }
}

void FakeRmadClient::TriggerCalibrationProgressObservation(
    rmad::RmadComponent component,
    rmad::CalibrationComponentStatus::CalibrationStatus status,
    double progress) {
  rmad::CalibrationComponentStatus componentStatus;
  componentStatus.set_component(component);
  componentStatus.set_status(status);
  componentStatus.set_progress(progress);
  for (auto& observer : observers_) {
    observer.CalibrationProgress(componentStatus);
  }
}

void FakeRmadClient::TriggerCalibrationOverallProgressObservation(
    rmad::CalibrationOverallStatus status) {
  for (auto& observer : observers_) {
    observer.CalibrationOverallProgress(status);
  }
}

void FakeRmadClient::TriggerProvisioningProgressObservation(
    rmad::ProvisionStatus::Status status,
    double progress,
    rmad::ProvisionStatus::Error error) {
  rmad::ProvisionStatus status_proto;
  status_proto.set_status(status);
  status_proto.set_progress(progress);
  status_proto.set_error(error);
  for (auto& observer : observers_) {
    observer.ProvisioningProgress(status_proto);
  }
}

void FakeRmadClient::TriggerHardwareWriteProtectionStateObservation(
    bool enabled) {
  for (auto& observer : observers_) {
    observer.HardwareWriteProtectionState(enabled);
  }
}

void FakeRmadClient::TriggerPowerCableStateObservation(bool plugged_in) {
  for (auto& observer : observers_) {
    observer.PowerCableState(plugged_in);
  }
}

void FakeRmadClient::TriggerExternalDiskStateObservation(bool detected) {
  for (auto& observer : observers_) {
    observer.ExternalDiskState(detected);
  }
}

void FakeRmadClient::TriggerHardwareVerificationResultObservation(
    bool is_compliant,
    const std::string& error_str) {
  rmad::HardwareVerificationResult verificationStatus;
  verificationStatus.set_is_compliant(is_compliant);
  verificationStatus.set_error_str(error_str);
  for (auto& observer : observers_) {
    observer.HardwareVerificationResult(verificationStatus);
  }
}

void FakeRmadClient::TriggerFinalizationProgressObservation(
    rmad::FinalizeStatus::Status status,
    double progress,
    rmad::FinalizeStatus::Error error) {
  rmad::FinalizeStatus finalizationStatus;
  finalizationStatus.set_status(status);
  finalizationStatus.set_progress(progress);
  finalizationStatus.set_error(error);
  for (auto& observer : observers_) {
    observer.FinalizationProgress(finalizationStatus);
  }
}

void FakeRmadClient::TriggerRoFirmwareUpdateProgressObservation(
    rmad::UpdateRoFirmwareStatus status) {
  for (auto& observer : observers_) {
    observer.RoFirmwareUpdateProgress(status);
  }
}

const rmad::GetStateReply& FakeRmadClient::GetStateReply() const {
  return state_replies_[state_index_];
}

const rmad::RmadState& FakeRmadClient::GetState() const {
  return GetStateReply().state();
}

rmad::RmadState::StateCase FakeRmadClient::GetStateCase() const {
  return GetState().state_case();
}

size_t FakeRmadClient::NumStates() const {
  return state_replies_.size();
}

}  // namespace ash
