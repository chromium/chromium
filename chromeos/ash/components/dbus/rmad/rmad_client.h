// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_RMAD_RMAD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_RMAD_RMAD_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/rmad/rmad.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace dbus {
class Bus;
}

namespace ash {

// RmadClient is responsible for receiving D-bus signals from the RmaDaemon
// service. The RmaDaemon is the underlying service that informs us whenever
// a shimless RMA is in progress and manages its state.
// Shimless RMA implements repair finalization for devices without the use of
// the USB shim. See go/cros-shimless-rma for details.
class COMPONENT_EXPORT(RMAD) RmadClient {
 public:
  // Interface for observing signals from rmad.
  class Observer : public base::CheckedObserver {
   public:
    // Called when an error occurs outside of state transitions.
    // e.g. while calibrating devices.
    virtual void Error(rmad::RmadErrorCode error) {}

    // Called when calibration progress is updated.
    virtual void CalibrationProgress(
        const rmad::CalibrationComponentStatus& component_status) {}

    // Called when overall calibration progress is updated.
    virtual void CalibrationOverallProgress(
        rmad::CalibrationOverallStatus status) {}

    // Called when provisioning progress is updated.
    virtual void ProvisioningProgress(const rmad::ProvisionStatus& status) {}

    // Called when hardware write protection state changes.
    virtual void HardwareWriteProtectionState(bool enabled) {}

    // Called when power cable is plugged in or removed.
    virtual void PowerCableState(bool plugged_in) {}

    // Called when an external disk is plugged in or removed.
    virtual void ExternalDiskState(bool detected) {}

    // Called when hardware verification completes.
    virtual void HardwareVerificationResult(
        const rmad::HardwareVerificationResult& result) {}

    // Called when finalization progress is updated.
    virtual void FinalizationProgress(const rmad::FinalizeStatus& status) {}

    // Called when overall calibration progress is updated.
    virtual void RoFirmwareUpdateProgress(rmad::UpdateRoFirmwareStatus status) {
    }
  };

  // Creates and initializes a global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static RmadClient* Get();

  // Returns true if RMA is supported and the RMA state files were detected.
  virtual bool WasRmaStateDetected() = 0;

  // Called by ChromeSessionManager, `session_manager_callback` is invoked when
  // the RMA check completes and it's determined that RMA is required.
  virtual void SetRmaRequiredCallbackForSessionManager(
      base::OnceClosure session_manager_callback) = 0;

  // Asynchronously gets the current RMA state.
  // The response contains an error code and the current state of the RMA
  // process.
  virtual void GetCurrentState(
      chromeos::DBusMethodCallback<rmad::GetStateReply> callback) = 0;
  // Asynchronously attempts to transition to the next RMA state.
  // The response contains an error code and the current state of the RMA
  // process.
  virtual void TransitionNextState(
      const rmad::RmadState& state,
      chromeos::DBusMethodCallback<rmad::GetStateReply> callback) = 0;
  // Asynchronously attempts to transition to the previous RMA state.
  // The response contains an error code and the current state of the RMA
  // process.
  virtual void TransitionPreviousState(
      chromeos::DBusMethodCallback<rmad::GetStateReply> callback) = 0;

  // Request the RMA process be cancelled.
  // There is no guarantee the callback is called if abort is successful because
  // the device will reboot.
  // Returns RMAD_ERROR_OK on success or an error code.
  virtual void AbortRma(
      chromeos::DBusMethodCallback<rmad::AbortRmaReply> callback) = 0;

  // Request the RMA process logs.
  virtual void GetLog(
      chromeos::DBusMethodCallback<rmad::GetLogReply> callback) = 0;

  // Save RMA logs to a USB drive.
  virtual void SaveLog(
      const std::string& diagnostics_log_text,
      chromeos::DBusMethodCallback<rmad::SaveLogReply> callback) = 0;

  // Send metrics to the platform side, which will upload them.
  virtual void RecordBrowserActionMetric(
      const rmad::RecordBrowserActionMetricRequest request,
      chromeos::DBusMethodCallback<rmad::RecordBrowserActionMetricReply>
          callback) = 0;

  // Extracts the diagnostics app from external sources.
  virtual void ExtractExternalDiagnosticsApp(
      chromeos::DBusMethodCallback<rmad::ExtractExternalDiagnosticsAppReply>
          callback) = 0;

  // Installs the diagnostics app extracted by last
  // `ExtractExternalDiagnosticsApp` call.
  virtual void InstallExtractedDiagnosticsApp(
      chromeos::DBusMethodCallback<rmad::InstallExtractedDiagnosticsAppReply>
          callback) = 0;

  // Gets the installed diagnostics app.
  virtual void GetInstalledDiagnosticsApp(
      chromeos::DBusMethodCallback<rmad::GetInstalledDiagnosticsAppReply>
          callback) = 0;

  // Adds and removes the observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(const Observer* observer) const = 0;

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

 protected:
  // Initialize/Shutdown should be used instead.
  RmadClient();

  RmadClient(const RmadClient&) = delete;
  RmadClient& operator=(const RmadClient&) = delete;
  virtual ~RmadClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_RMAD_RMAD_CLIENT_H_
