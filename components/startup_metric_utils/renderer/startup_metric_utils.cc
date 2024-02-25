// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/renderer/startup_metric_utils.h"

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

RendererStartupMetricRecorder& GetRenderer() {
  // If this ceases to be true, Get{Common,Renderer,Browser} need to be changed
  // to use base::NoDestructor.
  static_assert(
      std::is_trivially_destructible<RendererStartupMetricRecorder>::value,
      "Startup metric recorder classes must be trivially destructible.");
  // The GPU service gets created in browser tests, where we are not in the gpu
  // process, and so gating by --type=gpu-process being present on the command
  // line doesn't make sense.
  static RendererStartupMetricRecorder instance;
  return instance;
}

void RendererStartupMetricRecorder::RecordRunLoopStart(base::TimeTicks ticks) {
  if (GetCommon().process_creation_ticks_.is_null() ||
      GetCommon().application_start_ticks_.is_null() ||
      GetCommon().chrome_main_entry_ticks_.is_null()) {
    // This occurs in tests, content_shell.exe runs a renderer through another
    // entry point which does not record startup metrics.
    return;
  }

  GetCommon().AssertFirstCallInSession(FROM_HERE);

  run_loop_start_ticks_ = ticks;

  UmaHistogramWithTrace(
      &base::UmaHistogramMediumTimes,
      "Startup.Renderer.LoadTime.ProcessCreationToRendererStartRunLoop",
      GetCommon().process_creation_ticks_, run_loop_start_ticks_);
  UmaHistogramWithTrace(
      &base::UmaHistogramMediumTimes,
      "Startup.Renderer.LoadTime.ApplicationStartToRendererStartRunLoop",
      GetCommon().application_start_ticks_, run_loop_start_ticks_);
  UmaHistogramWithTrace(
      &base::UmaHistogramMediumTimes,
      "Startup.Renderer.LoadTime.ChromeMainToRendererStartRunLoop",
      GetCommon().chrome_main_entry_ticks_, run_loop_start_ticks_);
}

}  // namespace startup_metric_utils
