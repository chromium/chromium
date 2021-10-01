// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/rmad/fake_rmad_client.h"

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {
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
    case rmad::RmadState::kVerifyRsu:
      state->set_allocated_verify_rsu(new rmad::VerifyRsuState());
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
    default:
      NOTREACHED();
      break;
  }
  return state;
}

rmad::GetStateReply CreateStateReply(rmad::RmadState::StateCase state,
                                     rmad::RmadErrorCode error) {
  rmad::GetStateReply reply;
  reply.set_allocated_state(CreateState(state));
  reply.set_error(error);
  return reply;
}
}  // namespace

/* static */
void FakeRmadClient::CreateWithState() {
  FakeRmadClient* fake = new FakeRmadClient();
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

  std::vector<rmad::GetStateReply> fake_states = {
      CreateStateReply(rmad::RmadState::kWelcome, rmad::RMAD_ERROR_OK),
      components_repair_state,
      CreateStateReply(rmad::RmadState::kDeviceDestination,
                       rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWpDisableMethod, rmad::RMAD_ERROR_OK),
      wp_disable_rsu_state,
      CreateStateReply(rmad::RmadState::kVerifyRsu, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWpDisablePhysical,
                       rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWpDisableComplete,
                       rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kUpdateRoFirmware, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kRestock, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kUpdateDeviceInfo, rmad::RMAD_ERROR_OK),
      // TODO(gavindodd): Add calibration states when implemented.
      // rmad::RmadState::kCheckCalibration
      // rmad::RmadState::kSetupCalibration
      // rmad::RmadState::kRunCalibration
      CreateStateReply(rmad::RmadState::kProvisionDevice, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kWpEnablePhysical, rmad::RMAD_ERROR_OK),
      CreateStateReply(rmad::RmadState::kFinalize, rmad::RMAD_ERROR_OK),
  };
  fake->SetFakeStateReplies(fake_states);
}

FakeRmadClient::FakeRmadClient() {
  abort_rma_reply_.set_error(rmad::RMAD_ERROR_OK);
}
FakeRmadClient::~FakeRmadClient() = default;

void FakeRmadClient::GetCurrentState(
    DBusMethodCallback<rmad::GetStateReply> callback) {
  if (NumStates() > 0) {
    CHECK(state_index_ < NumStates());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), GetStateReply()));
  } else {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
  }
}

void FakeRmadClient::TransitionNextState(
    const rmad::RmadState& state,
    DBusMethodCallback<rmad::GetStateReply> callback) {
  if (NumStates() == 0) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
    return;
  }
  CHECK_LT(state_index_, NumStates());
  if (state.state_case() != GetStateCase()) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_REQUEST_INVALID);
    reply.set_allocated_state(new rmad::RmadState(GetState()));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
    return;
  }
  if (state_index_ >= NumStates() - 1) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_TRANSITION_FAILED);
    reply.set_allocated_state(new rmad::RmadState(GetState()));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GetStateReply()));
}

void FakeRmadClient::TransitionPreviousState(
    DBusMethodCallback<rmad::GetStateReply> callback) {
  if (NumStates() == 0) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_RMA_NOT_REQUIRED);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
    return;
  }
  CHECK_LT(state_index_, NumStates());
  if (state_index_ == 0) {
    rmad::GetStateReply reply;
    reply.set_error(rmad::RMAD_ERROR_TRANSITION_FAILED);
    reply.set_allocated_state(new rmad::RmadState(GetState()));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(reply)));
    return;
  }
  state_index_--;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), GetStateReply()));
}

void FakeRmadClient::AbortRma(
    DBusMethodCallback<rmad::AbortRmaReply> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     absl::optional<rmad::AbortRmaReply>(abort_rma_reply_)));
}

void FakeRmadClient::GetLogPath(DBusMethodCallback<std::string> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     absl::optional<std::string>("fake/log/path.log")));
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

void FakeRmadClient::SetFakeStateReplies(
    std::vector<rmad::GetStateReply> fake_states) {
  state_replies_ = std::move(fake_states);
  state_index_ = 0;
}

void FakeRmadClient::SetAbortable(bool abortable) {
  abort_rma_reply_.set_error(abortable ? rmad::RMAD_ERROR_OK
                                       : rmad::RMAD_ERROR_CANNOT_CANCEL_RMA);
}

void FakeRmadClient::TriggerErrorObservation(rmad::RmadErrorCode error) {
  for (auto& observer : observers_)
    observer.Error(error);
}

void FakeRmadClient::TriggerCalibrationProgressObservation(
    rmad::RmadComponent component,
    rmad::CalibrationComponentStatus::CalibrationStatus status,
    double progress) {
  rmad::CalibrationComponentStatus componentStatus;
  componentStatus.set_component(component);
  componentStatus.set_status(status);
  componentStatus.set_progress(progress);
  for (auto& observer : observers_)
    observer.CalibrationProgress(componentStatus);
}

void FakeRmadClient::TriggerCalibrationOverallProgressObservation(
    rmad::CalibrationOverallStatus status) {
  for (auto& observer : observers_)
    observer.CalibrationOverallProgress(status);
}

void FakeRmadClient::TriggerProvisioningProgressObservation(
    rmad::ProvisionDeviceState::ProvisioningStep step,
    double progress) {
  for (auto& observer : observers_)
    observer.ProvisioningProgress(step, progress);
}

void FakeRmadClient::TriggerHardwareWriteProtectionStateObservation(
    bool enabled) {
  for (auto& observer : observers_)
    observer.HardwareWriteProtectionState(enabled);
}

void FakeRmadClient::TriggerPowerCableStateObservation(bool plugged_in) {
  for (auto& observer : observers_)
    observer.PowerCableState(plugged_in);
}

void FakeRmadClient::TriggerHardwareVerificationResultObservation(
    bool is_compliant,
    const std::string& error_str) {
  rmad::HardwareVerificationResult verificationStatus;
  verificationStatus.set_is_compliant(is_compliant);
  verificationStatus.set_error_str(error_str);
  for (auto& observer : observers_)
    observer.HardwareVerificationResult(verificationStatus);
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

}  // namespace chromeos
