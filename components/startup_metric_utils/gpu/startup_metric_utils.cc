// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/gpu/startup_metric_utils.h"

#include <type_traits>

#include "base/check.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"


namespace startup_metric_utils {

GpuStartupMetricRecorder& GetGpu() {
  // If this ceases to be true, Get{Common,Gpu,Browser} need to be changed to
  // use base::NoDestructor.
  static_assert(
      std::is_trivially_destructible<GpuStartupMetricRecorder>::value,
      "Startup metric recorder classes must be trivially destructible.");
  // The GPU service gets created in browser tests, where we are not in the gpu
  // process, and so gating by --type=gpu-process being present on the command
  // line doesn't make sense.
  static GpuStartupMetricRecorder instance;
  return instance;
}

void GpuStartupMetricRecorder::RecordGpuInitializationTicks(
    base::TimeTicks ticks) {
  DCHECK(gpu_initialized_ticks_.is_null());
  gpu_initialized_ticks_ = ticks;
  DCHECK(!gpu_initialized_ticks_.is_null());
}
void GpuStartupMetricRecorder::RecordGpuInitialized(base::TimeTicks ticks) {
  if (GetCommon().process_creation_ticks_.is_null() ||
      GetCommon().application_start_ticks_.is_null() ||
      GetCommon().chrome_main_entry_ticks_.is_null()) {
    // This gets called on GPU initialization which occurs in contexts other
    // than Chrome launch (such as content tests), and in these cases, we
    // haven't initialized these values, though this is expected.
    return;
  }
  GetCommon().AssertFirstCallInSession(FROM_HERE);
  RecordGpuInitializationTicks(ticks);

  GetCommon().EmitHistogramWithTraceEvent(
      &base::UmaHistogramMediumTimes,
      "Startup.GPU.LoadTime.ProcessCreationToGpuInitialized",
      GetCommon().process_creation_ticks_, gpu_initialized_ticks_);
  GetCommon().EmitHistogramWithTraceEvent(
      &base::UmaHistogramMediumTimes,
      "Startup.GPU.LoadTime.ApplicationStartToGpuInitialized",
      GetCommon().application_start_ticks_, gpu_initialized_ticks_);
  GetCommon().EmitHistogramWithTraceEvent(
      &base::UmaHistogramMediumTimes,
      "Startup.GPU.LoadTime.ChromeMainToGpuInitialized",
      GetCommon().chrome_main_entry_ticks_, gpu_initialized_ticks_);
  if (!GetCommon().preread_end_ticks_.is_null() &&
      !GetCommon().preread_begin_ticks_.is_null()) {
    GetCommon().EmitHistogramWithTraceEvent(
        &base::UmaHistogramLongTimes, "Startup.GPU.LoadTime.PreReadFile",
        GetCommon().preread_begin_ticks_, GetCommon().preread_end_ticks_);
  }
}

}  // namespace startup_metric_utils
