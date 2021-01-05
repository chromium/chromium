// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_HISTOGRAM_UTIL_H_
#define CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_HISTOGRAM_UTIL_H_

#include <cstdint>

namespace chromeos {
namespace diagnostics {
namespace metrics {

void EmitRoutineRunCount(uint16_t routine_count);

}  // namespace metrics
}  // namespace diagnostics
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_DIAGNOSTICS_UI_BACKEND_HISTOGRAM_UTIL_H_
