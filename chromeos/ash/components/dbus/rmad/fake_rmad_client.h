// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_RMAD_FAKE_RMAD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_RMAD_FAKE_RMAD_CLIENT_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/rmad/rmad.pb.h"
#include "chromeos/ash/components/dbus/rmad/rmad_client.h"

namespace ash {

class COMPONENT_EXPORT(RMAD) FakeRmadClient : public RmadClient {
 public:
  FakeRmadClient();
  FakeRmadClient(const FakeRmadClient&) = delete;
  FakeRmadClient& operator=(const FakeRmadClient&) = delete;
  ~FakeRmadClient() override;

  // Returns the fake global instance if initialized. May return null.
  static FakeRmadClient* Get();

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

  void SetFakeStates();
  void SetFakeStateReplies(std::vector<rmad::GetStateReply> fake_states);

  void SetAbortable(bool abortable);
  void SetGetLogReply(const std::string& log, rmad::RmadErrorCode error);
  void SetSaveLogReply(const std::string& save_path, rmad::RmadErrorCode error);
  void SetRecordBrowserActionMetricReply(rmad::RmadErrorCode error);

  std::string GetDiagnosticsLogsText() const;

  base::FilePath& external_diag_app_path() { return external_diag_app_path_; }
  base::FilePath& installed_diag_app_path() { return installed_diag_app_path_; }

  void TriggerErrorObservation(rmad::RmadErrorCode error);
  void TriggerCalibrationProgressObservation(
      rmad::RmadComponent component,
      rmad::CalibrationComponentStatus::CalibrationStatus status,
      double progress);
  void TriggerCalibrationOverallProgressObservation(
      rmad::CalibrationOverallStatus status);
  void TriggerProvisioningProgressObservation(
      rmad::ProvisionStatus::Status status,
      double progress,
      rmad::ProvisionStatus::Error error);
  void TriggerHardwareWriteProtectionStateObservation(bool enabled);
  void TriggerPowerCableStateObservation(bool plugged_in);
  void TriggerExternalDiskStateObservation(bool detected_);
  void TriggerHardwareVerificationResultObservation(
      bool is_compliant,
      const std::string& error_str);
  void TriggerFinalizationProgressObservation(
      rmad::FinalizeStatus::Status status,
      double progress,
      rmad::FinalizeStatus::Error error);
  void TriggerRoFirmwareUpdateProgressObservation(
      rmad::UpdateRoFirmwareStatus status);

 private:
  const rmad::GetStateReply& GetStateReply() const;
  const rmad::RmadState& GetState() const;
  rmad::RmadState::StateCase GetStateCase() const;
  size_t NumStates() const;

  std::vector<rmad::GetStateReply> state_replies_;
  size_t state_index_;
  rmad::AbortRmaReply abort_rma_reply_;
  rmad::GetLogReply get_log_reply_;
  rmad::SaveLogReply save_log_reply_;
  rmad::RecordBrowserActionMetricReply record_browser_action_metric_reply_;
  base::ObserverList<Observer, /*check_empty=*/true, /*allow_reentrancy=*/false>
      observers_;
  std::string diagnostics_logs_text_;

  base::FilePath external_diag_app_path_;
  base::FilePath installed_diag_app_path_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_RMAD_FAKE_RMAD_CLIENT_H_
