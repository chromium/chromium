// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_ROUTINE_CONTROLLER_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_ROUTINE_CONTROLLER_H_

#include "chromeos/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace diagnostics {

class SystemRoutineController {
 public:
  SystemRoutineController();
  ~SystemRoutineController();

  SystemRoutineController(const SystemRoutineController&) = delete;
  SystemRoutineController& operator=(const SystemRoutineController&) = delete;

 private:
  void BindCrosHealthdDiagnosticsServiceIfNeccessary();

  void OnDiagnosticsServiceDisconnected();

  mojo::Remote<cros_healthd::mojom::CrosHealthdDiagnosticsService>
      diagnostics_service_;
};

}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_SYSTEM_ROUTINE_CONTROLLER_H_
