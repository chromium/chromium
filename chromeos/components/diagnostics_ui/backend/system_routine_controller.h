// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_ROUTINE_CONTROLLER_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_ROUTINE_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chromeos/components/diagnostics_ui/mojom/system_routine_controller.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace cros_healthd {
namespace mojom {
class RunRoutineResponsePtr;
class RoutineUpdatePtr;
}  // namespace mojom
}  // namespace cros_healthd

namespace chromeos {
namespace diagnostics {

constexpr int32_t kInvalidRoutineId = 0;

using RunRoutineCallback =
    base::OnceCallback<void(cros_healthd::mojom::RunRoutineResponsePtr)>;

class SystemRoutineController : public mojom::SystemRoutineController {
 public:
  SystemRoutineController();
  ~SystemRoutineController() override;

  SystemRoutineController(const SystemRoutineController&) = delete;
  SystemRoutineController& operator=(const SystemRoutineController&) = delete;

  // mojom::SystemRoutineController:
  void GetSupportedRoutines(GetSupportedRoutinesCallback callback) override;
  void RunRoutine(mojom::RoutineType type,
                  mojo::PendingRemote<mojom::RoutineRunner> runner) override;

  void BindInterface(
      mojo::PendingReceiver<mojom::SystemRoutineController> pending_receiver);

 private:
  void OnAvailableRoutinesFetched(
      GetSupportedRoutinesCallback callback,
      const std::vector<cros_healthd::mojom::DiagnosticRoutineEnum>&
          supported_routines);

  void ExecuteRoutine(mojom::RoutineType routine_type);

  void OnRoutineStarted(
      mojom::RoutineType routine_type,
      cros_healthd::mojom::RunRoutineResponsePtr response_ptr);

  void OnPowerRoutineStarted(
      mojom::RoutineType routine_type,
      cros_healthd::mojom::RunRoutineResponsePtr response_ptr);

  void ContinuePowerRoutine(mojom::RoutineType routine_type);

  void OnPowerRoutineContinued(
      mojom::RoutineType routine_type,
      cros_healthd::mojom::RoutineUpdatePtr update_ptr);

  void CheckRoutineStatus(mojom::RoutineType routine_type);

  void OnRoutineStatusUpdated(mojom::RoutineType routine_type,
                              cros_healthd::mojom::RoutineUpdatePtr update_ptr);

  void HandlePowerRoutineStatusUpdate(
      mojom ::RoutineType routine_type,
      cros_healthd::mojom::RoutineUpdatePtr update_ptr);

  bool IsRoutineRunning() const;

  void ScheduleCheckRoutineStatus(uint32_t duration_in_seconds,
                                  mojom::RoutineType routine_type);

  void ParsePowerRoutineResult(mojom::RoutineType routine_type,
                               mojom::StandardRoutineResult result,
                               mojo::ScopedHandle output_handle);

  void OnPowerRoutineResultFetched(mojom::RoutineType routine_type,
                                   const std::string& file_contents);

  void OnPowerRoutineJsonParsed(mojom::RoutineType routine_type,
                                data_decoder::DataDecoder::ValueOrError result);

  void OnStandardRoutineResult(mojom::RoutineType routine_type,
                               mojom::StandardRoutineResult result);

  void OnPowerRoutineResult(mojom::RoutineType routine_type,
                            mojom::StandardRoutineResult result,
                            double percent_change,
                            uint32_t seconds_elapsed);

  void SendRoutineResult(mojom::RoutineResultInfoPtr result_info);

  void BindCrosHealthdDiagnosticsServiceIfNeccessary();

  void OnDiagnosticsServiceDisconnected();

  void OnInflightRoutineRunnerDisconnected();

  void OnRoutineCancelAttempted(
      cros_healthd::mojom::RoutineUpdatePtr update_ptr);

  // Keeps track of the id created by CrosHealthd for the currently running
  // routine.
  int32_t inflight_routine_id_ = kInvalidRoutineId;

  // Records the number of routines that a user attempts to run during one
  // session in the app. Emitted when the app is closed.
  uint16_t routine_count_ = 0;

  mojo::Remote<mojom::RoutineRunner> inflight_routine_runner_;
  std::unique_ptr<base::OneShotTimer> inflight_routine_timer_;

  mojo::Remote<cros_healthd::mojom::CrosHealthdDiagnosticsService>
      diagnostics_service_;

  mojo::Receiver<mojom::SystemRoutineController> receiver_{this};

  base::WeakPtrFactory<SystemRoutineController> weak_factory_{this};
};

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_ROUTINE_CONTROLLER_H_
