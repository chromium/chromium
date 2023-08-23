// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/gpu/startup_metric_utils.h"

#include <type_traits>

#include "base/check.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"

namespace {
void UmaHistogramWithTrace(void (*histogram_function)(const std::string& name,
                                                      base::TimeDelta),
                           const char* histogram_basename,
                           base::TimeTicks begin_ticks,
                           base::TimeTicks end_ticks) {
  (*histogram_function)(histogram_basename, end_ticks - begin_ticks);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "startup", histogram_basename, TRACE_ID_WITH_SCOPE(histogram_basename, 0),
      begin_ticks);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "startup", histogram_basename, TRACE_ID_WITH_SCOPE(histogram_basename, 0),
      end_ticks);
}
}  // namespace

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

  UmaHistogramWithTrace(&base::UmaHistogramMediumTimes,
                        "Startup.GPU.LoadTime.ProcessCreationToGpuInitialized",
                        GetCommon().process_creation_ticks_,
                        gpu_initialized_ticks_);
  UmaHistogramWithTrace(&base::UmaHistogramMediumTimes,
                        "Startup.GPU.LoadTime.ApplicationStartToGpuInitialized",
                        GetCommon().application_start_ticks_,
                        gpu_initialized_ticks_);
  UmaHistogramWithTrace(&base::UmaHistogramMediumTimes,
                        "Startup.GPU.LoadTime.ChromeMainToGpuInitialized",
                        GetCommon().chrome_main_entry_ticks_,
                        gpu_initialized_ticks_);
}

}  // namespace startup_metric_utils
