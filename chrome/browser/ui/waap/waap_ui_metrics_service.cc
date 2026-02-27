// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"

#include <string>
#include <string_view>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_recorder.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service_factory.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"

namespace {

std::string_view CreationSourceToString(waap::NewWindowCreationSource source) {
  switch (source) {
    case waap::NewWindowCreationSource::kSessionRestore:
      return ".SessionRestore";
    case waap::NewWindowCreationSource::kDragToNewWindow:
      return ".DragToNewWindow";
    case waap::NewWindowCreationSource::kBrowserInitiated:
      return ".BrowserInitiated";
    case waap::NewWindowCreationSource::kUnknown:
      NOTREACHED();
  }
  NOTREACHED();
}

std::string_view ReloadButtonModeToString(
    WaapUIMetricsRecorder::ReloadButtonMode mode) {
  switch (mode) {
    case WaapUIMetricsRecorder::ReloadButtonMode::kReload:
      return "Reload";
    case WaapUIMetricsRecorder::ReloadButtonMode::kStop:
      return "Stop";
  }
  NOTREACHED();
}

std::string_view ReloadButtonInputTypeToString(
    WaapUIMetricsRecorder::ReloadButtonInputType type) {
  switch (type) {
    case WaapUIMetricsRecorder::ReloadButtonInputType::kMouseRelease:
      return ".MouseRelease";
    case WaapUIMetricsRecorder::ReloadButtonInputType::kKeyPress:
      return ".KeyPress";
  }
  NOTREACHED();
}

// Helper to construct the full histogram name for ReloadButton metrics
std::string BuildReloadButtonHistogramName(std::string_view base,
                                           std::string_view slice = "") {
  return base::StrCat({"InitialWebUI.ReloadButton.", base, slice});
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(InitialWebUIView)
enum class InitialWebUIView {
  kBrowserWindow = 0,
  kReloadButton = 1,
  kMaxValue = kReloadButton,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ui/enums.xml:InitialWebUIView)

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

// Emits a WaaP trace event and records a UMA histogram with the given event
// name and duration.
void EmitReloadButtonHistogramWithTraceEvent(const char* event_name,
                                             base::TimeTicks start_ticks,
                                             base::TimeTicks end_ticks) {
  const base::TimeDelta duration = end_ticks - start_ticks;
  TRACE_EVENT_BEGIN("waap", perfetto::StaticString(event_name),
                    perfetto::Track(reinterpret_cast<uintptr_t>(event_name)),
                    start_ticks);
  TRACE_EVENT_END("waap",
                  perfetto::Track(reinterpret_cast<uintptr_t>(event_name)),
                  end_ticks);
  base::UmaHistogramCustomTimes(event_name, duration, base::Milliseconds(1),
                                base::Minutes(3), 100);
}

// Returns a suffix for the startup temperature of the browser.
const char* GetStartupTemperatureSuffix() {
  switch (startup_metric_utils::GetBrowser().GetStartupTemperature()) {
    case startup_metric_utils::COLD_STARTUP_TEMPERATURE:
      return ".Temperature.ColdStartup";
    case startup_metric_utils::WARM_STARTUP_TEMPERATURE:
      return ".Temperature.WarmStartup";
    case startup_metric_utils::LUKEWARM_STARTUP_TEMPERATURE:
    case startup_metric_utils::UNDETERMINED_STARTUP_TEMPERATURE:
      return ".Temperature.Other";
    case startup_metric_utils::STARTUP_TEMPERATURE_COUNT:
      NOTREACHED();
  }
  return ".Temperature.Other";
}

// Records a startup paint metric for the given `paint_metric_base`.
void RecordStartupPaintMetric(std::string_view paint_metric_base,
                              base::TimeTicks start_time,
                              base::TimeTicks paint_time) {
  if (!startup_metric_utils::GetBrowser().ShouldLogStartupHistogram() ||
      start_time.is_null() || paint_time.is_null()) {
    // This excludes the cases where profile picker is shown, background mode
    // is enabled, or OS displays other UI before browser window.
    return;
  }

  std::string scenario_suffix;
  if (startup_metric_utils::GetBrowser().IsFirstRun()) {
    scenario_suffix = ".FirstRun";
  } else if (SessionRestore::IsAnySessionRestored()) {
    scenario_suffix = ".SessionRestore";
  }

  std::string base_name = base::StrCat(
      {"InitialWebUI.Startup", scenario_suffix, ".", paint_metric_base});

  // Record aggregate metric.
  EmitHistogramWithTraceEvent(base_name.c_str(), start_time, paint_time);

  // Record temperature-sliced metric.
  if (const std::string_view temp_suffix = GetStartupTemperatureSuffix();
      !temp_suffix.empty()) {
    EmitHistogramWithTraceEvent(base::StrCat({base_name, temp_suffix}).c_str(),
                                start_time, paint_time);
  }
}

// Records a new window paint metric for the given `paint_metric_base`.
void RecordNewWindowPaintMetric(std::string_view paint_metric_base,
                                waap::NewWindowCreationSource source,
                                base::TimeTicks start_time,
                                base::TimeTicks paint_time) {
  // Record aggregated metric.
  EmitHistogramWithTraceEvent(
      base::StrCat({"InitialWebUI.NewWindow.AllSources.", paint_metric_base})
          .c_str(),
      start_time, paint_time);

  // Record source-sliced metric.
  std::string_view source_str = CreationSourceToString(source);
  EmitHistogramWithTraceEvent(base::StrCat({"InitialWebUI.NewWindow",
                                            source_str, ".", paint_metric_base})
                                  .c_str(),
                              start_time, paint_time);
}

}  // namespace

WaapUIMetricsService::WaapUIMetricsService(
    base::PassKey<WaapUIMetricsServiceFactory>,
    const Profile* profile) {}

WaapUIMetricsService::~WaapUIMetricsService() = default;

// static
WaapUIMetricsService* WaapUIMetricsService::Get(Profile* profile) {
  return WaapUIMetricsServiceFactory::GetForProfile(profile);
}

void WaapUIMetricsService::OnBrowserWindowCreated() {
  base::UmaHistogramEnumeration("InitialWebUI.View.Creation",
                                InitialWebUIView::kBrowserWindow);
}

void WaapUIMetricsService::OnReloadButtonCreated() {
  base::UmaHistogramEnumeration("InitialWebUI.View.Creation",
                                InitialWebUIView::kReloadButton);
}

void WaapUIMetricsService::OnBrowserWindowFirstPresentation(
    base::TimeTicks time) {
  static bool is_first_call = true;
  // It is possible for the presentation feedback to have a null timestamp even
  // if the presentation was considered successful (e.g. if the OS/driver
  // confirmed the swap but didn't provide a timestamp). In this case, we simply
  // skip recording the metric.
  // A longer term fix would require modifying
  // `CompositorFrameSinkSupport::DidPresentCompositorFrame()`, which requires
  // carefully auditing all callers. See https://crbug.com/464980749#comment10.
  if (time.is_null()) {
    return;
  }
  CHECK(is_first_call);
  is_first_call = false;

  base::TimeTicks time_origin =
      startup_metric_utils::GetBrowser().GetApplicationStartTicksForStartup();
  RecordStartupPaintMetric("BrowserWindow.FirstPaint", time_origin, time);
}

void WaapUIMetricsService::OnFirstPaint(base::TimeTicks time) {
  static bool is_first_call = true;
  // See https://crbug.com/464980749#comment10 for why we skip for null.
  if (time.is_null()) {
    return;
  }
  if (!is_first_call) {
    return;
  }
  is_first_call = false;

  // For early experiment, this is ReloadButton only.
  // TODO(crbug.com/448794588): Switch to general name after initial phase.
  base::TimeTicks time_origin =
      startup_metric_utils::GetBrowser().GetApplicationStartTicksForStartup();
  RecordStartupPaintMetric("ReloadButton.FirstPaint", time_origin, time);
}

void WaapUIMetricsService::OnFirstContentfulPaint(base::TimeTicks time) {
  static bool is_first_call = true;
  // See https://crbug.com/464980749#comment10 for why we skip for null.
  if (time.is_null()) {
    return;
  }
  if (!is_first_call) {
    return;
  }
  is_first_call = false;

  // For early experiment, this is ReloadButton only.
  // TODO(crbug.com/448794588): Switch to general name after initial phase.
  base::TimeTicks time_origin =
      startup_metric_utils::GetBrowser().GetApplicationStartTicksForStartup();
  RecordStartupPaintMetric("ReloadButton.FirstContentfulPaint", time_origin, time);
}

void WaapUIMetricsService::OnNewWindowBrowserWindowFirstPresentation(
    waap::NewWindowCreationSource source,
    base::TimeTicks start_time,
    base::TimeTicks paint_time) {
  if (start_time.is_null() || paint_time.is_null() ||
      source == waap::NewWindowCreationSource::kUnknown) {
    return;
  }

  RecordNewWindowPaintMetric("BrowserWindow.FirstPaint.FromConstructor", source,
                             start_time, paint_time);
}

void WaapUIMetricsService::OnNewWindowReloadButtonFirstPaint(
    waap::NewWindowCreationSource source,
    base::TimeTicks start_time,
    base::TimeTicks paint_time) {
  if (start_time.is_null() || paint_time.is_null() ||
      source == waap::NewWindowCreationSource::kUnknown) {
    return;
  }

  RecordNewWindowPaintMetric("ReloadButton.FirstPaint.FromConstructor", source,
                             start_time, paint_time);
}

void WaapUIMetricsService::OnNewWindowReloadButtonFirstContentfulPaint(
    waap::NewWindowCreationSource source,
    base::TimeTicks start_time,
    base::TimeTicks paint_time) {
  if (start_time.is_null() || paint_time.is_null() ||
      source == waap::NewWindowCreationSource::kUnknown) {
    return;
  }

  RecordNewWindowPaintMetric(
      "ReloadButton.FirstContentfulPaint.FromConstructor", source, start_time,
      paint_time);
}

void WaapUIMetricsService::OnStartupBrowserWindowToReloadButtonFirstPaintGap(
    base::TimeTicks browser_window_paint_time,
    base::TimeTicks reload_button_paint_time) {
  RecordStartupPaintMetric("BrowserWindowToReloadButton.FirstPaintGap",
                           browser_window_paint_time, reload_button_paint_time);
}

void WaapUIMetricsService::OnNewWindowBrowserWindowToReloadButtonFirstPaintGap(
    waap::NewWindowCreationSource source,
    base::TimeTicks browser_window_paint_time,
    base::TimeTicks reload_button_paint_time) {
  RecordNewWindowPaintMetric(
      "BrowserWindowToReloadButton.FirstPaintGap", source,
      browser_window_paint_time, reload_button_paint_time);
}

void WaapUIMetricsService::OnReloadButtonMousePressToNextPaint(
    base::TimeTicks start_ticks,
    base::TimeTicks end_ticks) {
  auto name = BuildReloadButtonHistogramName("MousePressToNextPaint");
  EmitReloadButtonHistogramWithTraceEvent(name.c_str(), start_ticks, end_ticks);
}

void WaapUIMetricsService::OnReloadButtonMouseHoverToNextPaint(
    base::TimeTicks start_ticks,
    base::TimeTicks end_ticks) {
  auto name = BuildReloadButtonHistogramName("MouseHoverToNextPaint");
  EmitReloadButtonHistogramWithTraceEvent(name.c_str(), start_ticks, end_ticks);
}

void WaapUIMetricsService::OnReloadButtonInput(
    WaapUIMetricsRecorder::ReloadButtonInputType input_type) {
  auto name = BuildReloadButtonHistogramName("InputCount");
  base::UmaHistogramEnumeration(name, input_type);
}

void WaapUIMetricsService::OnReloadButtonInputToReload(
    base::TimeTicks start_ticks,
    base::TimeTicks end_ticks,
    WaapUIMetricsRecorder::ReloadButtonInputType input_type) {
  auto name = BuildReloadButtonHistogramName(
      "InputToReload", ReloadButtonInputTypeToString(input_type));
  EmitReloadButtonHistogramWithTraceEvent(name.c_str(), start_ticks, end_ticks);
}

void WaapUIMetricsService::OnReloadButtonInputToStop(
    base::TimeTicks start_ticks,
    base::TimeTicks end_ticks,
    WaapUIMetricsRecorder::ReloadButtonInputType input_type) {
  auto name = BuildReloadButtonHistogramName(
      "InputToStop", ReloadButtonInputTypeToString(input_type));
  EmitReloadButtonHistogramWithTraceEvent(name.c_str(), start_ticks, end_ticks);
}

void WaapUIMetricsService::OnReloadButtonInputToNextPaint(
    base::TimeTicks start_ticks,
    base::TimeTicks end_ticks,
    WaapUIMetricsRecorder::ReloadButtonInputType input_type) {
  auto name = BuildReloadButtonHistogramName(
      "InputToNextPaint", ReloadButtonInputTypeToString(input_type));
  EmitReloadButtonHistogramWithTraceEvent(name.c_str(), start_ticks, end_ticks);
}

void WaapUIMetricsService::OnReloadButtonChangeVisibleModeToNextPaint(
    base::TimeTicks start_ticks,
    base::TimeTicks end_ticks,
    WaapUIMetricsRecorder::ReloadButtonMode new_mode) {
  auto name = BuildReloadButtonHistogramName(
      "ChangeVisibleModeToNextPaintIn", ReloadButtonModeToString(new_mode));
  EmitReloadButtonHistogramWithTraceEvent(name.c_str(), start_ticks, end_ticks);
}
