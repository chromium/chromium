// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/renderer/startup_metric_utils.h"

#include <type_traits>

#include "base/check.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"

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

  GetCommon().EmitHistogramWithTraceEvent(
      &base::UmaHistogramMediumTimes,
      "Startup.Renderer.LoadTime.ProcessCreationToRendererStartRunLoop",
      GetCommon().process_creation_ticks_, run_loop_start_ticks_);
  GetCommon().EmitHistogramWithTraceEvent(
      &base::UmaHistogramMediumTimes,
      "Startup.Renderer.LoadTime.ApplicationStartToRendererStartRunLoop",
      GetCommon().application_start_ticks_, run_loop_start_ticks_);
  GetCommon().EmitHistogramWithTraceEvent(
      &base::UmaHistogramMediumTimes,
      "Startup.Renderer.LoadTime.ChromeMainToRendererStartRunLoop",
      GetCommon().chrome_main_entry_ticks_, run_loop_start_ticks_);

  if (!GetCommon().preread_end_ticks_.is_null() &&
      !GetCommon().preread_begin_ticks_.is_null()) {
    GetCommon().EmitHistogramWithTraceEvent(
        &base::UmaHistogramLongTimes, "Startup.Renderer.LoadTime.PreReadFile",
        GetCommon().preread_begin_ticks_, GetCommon().preread_end_ticks_);
  }
}

}  // namespace startup_metric_utils
