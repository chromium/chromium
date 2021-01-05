// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/histogram_util.h"

#include "base/metrics/histogram_functions.h"

namespace chromeos {
namespace diagnostics {
namespace metrics {

void EmitRoutineRunCount(uint16_t routine_count) {
  base::UmaHistogramCounts100("ChromeOS.DiagnosticsUi.RoutineCount",
                              routine_count);
}

}  // namespace metrics
}  // namespace diagnostics
}  // namespace chromeos
