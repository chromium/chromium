// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service_factory.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"

namespace {

// Emits a WaaP trace event asynchronously onto a perfetto::Track and records a
// UMA histogram with the same event name.
void EmitHistogramWithTraceEvent(const char* event_name,
                                 base::TimeTicks start_ticks,
                                 base::TimeTicks end_ticks) {
  TRACE_EVENT_BEGIN("waap", perfetto::StaticString(event_name),
                    perfetto::Track(reinterpret_cast<uintptr_t>(event_name)),
                    start_ticks);
  TRACE_EVENT_END("waap",
                  perfetto::Track(reinterpret_cast<uintptr_t>(event_name)),
                  end_ticks);

  const base::TimeDelta delta = end_ticks - start_ticks;
  base::UmaHistogramLongTimes100(event_name, delta);
}

void RecordStartupPaintMetric(const char* paint_metric_name,
                              base::TimeTicks paint_time) {
  if (!startup_metric_utils::GetBrowser().ShouldLogStartupHistogram()) {
    return;
  }

  base::TimeTicks time_origin =
      startup_metric_utils::GetBrowser().GetApplicationStartTicksForStartup();
  if (time_origin.is_null()) {
    return;
  }

  // For early experiment, this is ReloadButton only.
  // TODO(crbug.com/448794588): Switch to general name after initial phase.
  std::string histogram_name = base::StrCat(
      {"InitialWebUI.Startup.ReloadButton.", paint_metric_name});
  switch (startup_metric_utils::GetBrowser().GetStartupTemperature()) {
    case startup_metric_utils::COLD_STARTUP_TEMPERATURE:
      histogram_name = base::StrCat({histogram_name, ".ColdStartup"});
      break;
    case startup_metric_utils::WARM_STARTUP_TEMPERATURE:
      histogram_name = base::StrCat({histogram_name, ".WarmStartup"});
      break;
    case startup_metric_utils::LUKEWARM_STARTUP_TEMPERATURE:
      break;
    case startup_metric_utils::UNDETERMINED_STARTUP_TEMPERATURE:
      break;
    case startup_metric_utils::STARTUP_TEMPERATURE_COUNT:
      NOTREACHED();
  }

  EmitHistogramWithTraceEvent(histogram_name.c_str(), time_origin, paint_time);
}

}  // namespace

WaapUIMetricsService::WaapUIMetricsService(
    base::PassKey<WaapUIMetricsServiceFactory>) {}

WaapUIMetricsService::~WaapUIMetricsService() = default;

// static
WaapUIMetricsService* WaapUIMetricsService::Get(Profile* profile) {
  return WaapUIMetricsServiceFactory::GetForProfile(profile);
}

void WaapUIMetricsService::OnFirstPaint(base::TimeTicks time) {
  static bool is_first_call = true;
  CHECK(!time.is_null());
  if (!is_first_call) {
    return;
  }
  is_first_call = false;

  RecordStartupPaintMetric("FirstPaint", time);
}

void WaapUIMetricsService::OnFirstContentfulPaint(base::TimeTicks time) {
  static bool is_first_call = true;
  CHECK(!time.is_null());
  if (!is_first_call) {
    return;
  }
  is_first_call = false;

  RecordStartupPaintMetric("FirstContentfulPaint", time);
}

