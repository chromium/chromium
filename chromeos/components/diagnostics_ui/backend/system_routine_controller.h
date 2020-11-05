// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_ROUTINE_CONTROLLER_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_ROUTINE_CONTROLLER_H_

#include <memory>

#include "chromeos/components/diagnostics_ui/mojom/system_routine_controller.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

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

using RunRoutineCallback =
    base::OnceCallback<void(cros_healthd::mojom::RunRoutineResponsePtr)>;

class SystemRoutineController : public mojom::SystemRoutineController {
 public:
  SystemRoutineController();
  ~SystemRoutineController() override;

  SystemRoutineController(const SystemRoutineController&) = delete;
  SystemRoutineController& operator=(const SystemRoutineController&) = delete;

  // mojom::SystemRoutineController:
  void RunRoutine(mojom::RoutineType type,
                  mojo::PendingRemote<mojom::RoutineRunner> runner) override;

 private:
  void ExecuteRoutine(mojom::RoutineType routine_type);

  void OnRoutineStarted(
      mojom::RoutineType routine_type,
      cros_healthd::mojom::RunRoutineResponsePtr response_ptr);

  void CheckRoutineStatus(mojom::RoutineType routine_type, int32_t id);

  void OnRoutineStatusUpdated(mojom::RoutineType routine_type,
                              int32_t id,
                              cros_healthd::mojom::RoutineUpdatePtr update_ptr);

  bool IsRoutineRunning() const;

  void ScheduleCheckRoutineStatus(uint32_t duration_in_seconds,
                                  mojom::RoutineType routine_type,
                                  int32_t id);

  void OnStandardRoutineResult(mojom::RoutineType routine_type,
                               mojom::StandardRoutineResult result);

  void BindCrosHealthdDiagnosticsServiceIfNeccessary();

  void OnDiagnosticsServiceDisconnected();

  void OnInflightRoutineRunnerDisconnected();

  mojo::Remote<mojom::RoutineRunner> inflight_routine_runner_;
  std::unique_ptr<base::OneShotTimer> inflight_routine_timer_;

  mojo::Remote<cros_healthd::mojom::CrosHealthdDiagnosticsService>
      diagnostics_service_;

  mojo::Receiver<mojom::SystemRoutineController> receiver_{this};
};

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_ROUTINE_CONTROLLER_H_
