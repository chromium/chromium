// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_recorder.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service_factory.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace {

// Tracks process-wide startup metrics. Startup metrics must be recorded at
// most once per browser process lifecycle across all windows and profiles.
// Declared in the anonymous namespace so `ResetForTesting()` can reset state
// between tests.
bool g_is_browser_window_first_paint_recorded = false;
bool g_is_first_paint_recorded = false;
bool g_is_first_contentful_paint_recorded = false;

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

std::string_view ExistingWindowToString(bool with_existing_window) {
  return with_existing_window ? "WithExistingWindow" : "WithoutExistingWindow";
}

const char* GetSurfaceSyncSuffix() {
  return base::FeatureList::IsEnabled(blink::features::kInitialWebUISurfaceSync)
             ? ".SurfaceSyncEnabled"
             : ".SurfaceSyncDisabled";
}

// Helper to construct the full histogram name for ReloadButton metrics
std::string BuildReloadButtonHistogramName(std::string_view base,
                                           std::string_view slice = "") {
  return base::StrCat({"InitialWebUI.ReloadButton.", base, slice});
}

void RecordReloadButtonPaintDuration(std::string_view name,
                                     base::TimeDelta delta,
                                     std::string_view slice = "") {
  base::UmaHistogramCustomTimes(BuildReloadButtonHistogramName(name, slice),
                                std::max(base::TimeDelta(), delta),
                                base::Milliseconds(1), base::Minutes(3), 100);
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

// Emits an Initial WebUI trace event asynchronously onto a perfetto::Track and
// records a UMA histogram with the same event name.
void EmitHistogramWithTraceEvent(const char* event_name,
                                 base::TimeTicks start_ticks,
                                 base::TimeTicks end_ticks) {
  auto track = perfetto::NamedTrack(perfetto::DynamicString(event_name));
  if (perfetto::Tracing::IsInitialized()) {
    base::TrackEvent::SetTrackDescriptor(track, track.Serialize());
  }
  TRACE_EVENT_BEGIN("waap", perfetto::DynamicString(event_name), track,
                    start_ticks);
  TRACE_EVENT_END("waap", track, end_ticks);

  const base::TimeDelta delta = end_ticks - start_ticks;
  base::UmaHistogramLongTimes100(event_name, delta);
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

  if (startup_metric_utils::GetBrowser().IsFirstRun()) {
    return;
  }

  std::string scenario_suffix;
  if (SessionRestore::IsAnySessionRestored()) {
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

// Records a new window paint metric which differentiates whether the new window
// is for a profile with or without existing browser window for the given
// `paint_metric_base`.
void RecordNewWindowPaintMetric(std::string_view paint_metric_base,
                                waap::NewWindowCreationSource source,
                                bool with_existing_window,
                                base::TimeTicks start_time,
                                base::TimeTicks paint_time) {
  const std::string_view with_existing_window_str =
      ExistingWindowToString(with_existing_window);

  // Record aggregated metric.
  EmitHistogramWithTraceEvent(
      base::StrCat({"InitialWebUI.NewWindow.AllSources.",
                    with_existing_window_str, ".", paint_metric_base})
          .c_str(),
      start_time, paint_time);

  // Record source-sliced metric.
  std::string_view source_str = CreationSourceToString(source);
  EmitHistogramWithTraceEvent(
      base::StrCat({"InitialWebUI.NewWindow", source_str, ".",
                    with_existing_window_str, ".", paint_metric_base})
          .c_str(),
      start_time, paint_time);
}

}  // namespace

WaapUIMetricsService::WaapUIMetricsService(
    base::PassKey<WaapUIMetricsServiceFactory>) {}

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

void WaapUIMetricsService::OnReloadButtonRendererProcessCreatedAndLaunched(
    base::TimeTicks created_timestamp,
    base::TimeTicks launched_timestamp) {
  // TODO(crbug.com/490810407): Record this and the other metrics as UKM as
  // well, so that we can see the progression of renderer process creation
  // requested
  // -> launched -> commit -> paint etc. UKM recording for topchrome is
  // currently not working.
  base::TimeTicks time_origin =
      startup_metric_utils::GetBrowser().GetApplicationStartTicksForStartup();
  if (!created_timestamp.is_null()) {
    RecordStartupPaintMetric("ReloadButton.RendererProcessCreated", time_origin,
                             created_timestamp);
  }
  if (!launched_timestamp.is_null()) {
    RecordStartupPaintMetric("ReloadButton.RendererProcessLaunched",
                             time_origin, launched_timestamp);
  }
}

// static
void WaapUIMetricsService::ResetForTesting() {
  g_is_browser_window_first_paint_recorded = false;
  g_is_first_paint_recorded = false;
  g_is_first_contentful_paint_recorded = false;
  startup_metric_utils::GetBrowser().ResetSessionForTesting();
}

void WaapUIMetricsService::OnBrowserWindowFirstPresentation(
    base::TimeTicks time) {
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
  CHECK(!g_is_browser_window_first_paint_recorded);
  g_is_browser_window_first_paint_recorded = true;

  base::TimeTicks time_origin =
      startup_metric_utils::GetBrowser().GetApplicationStartTicksForStartup();
  RecordStartupPaintMetric("BrowserWindow.FirstPaint", time_origin, time);
}

void WaapUIMetricsService::OnFirstPaint(base::TimeTicks time) {
  // See https://crbug.com/464980749#comment10 for why we skip for null.
  if (time.is_null()) {
    return;
  }
  if (g_is_first_paint_recorded) {
    return;
  }
  g_is_first_paint_recorded = true;

  // For early experiment, this is ReloadButton only.
  // TODO(crbug.com/448794588): Switch to general name after initial phase.
  base::TimeTicks time_origin =
      startup_metric_utils::GetBrowser().GetApplicationStartTicksForStartup();
  RecordStartupPaintMetric("ReloadButton.FirstPaint", time_origin, time);
}

void WaapUIMetricsService::OnFirstContentfulPaint(base::TimeTicks time) {
  // See https://crbug.com/464980749#comment10 for why we skip for null.
  if (time.is_null()) {
    return;
  }
  if (g_is_first_contentful_paint_recorded) {
    return;
  }
  g_is_first_contentful_paint_recorded = true;

  // For early experiment, this is ReloadButton only.
  // TODO(crbug.com/448794588): Switch to general name after initial phase.
  base::TimeTicks time_origin =
      startup_metric_utils::GetBrowser().GetApplicationStartTicksForStartup();
  RecordStartupPaintMetric("ReloadButton.FirstContentfulPaint", time_origin,
                           time);
}

void WaapUIMetricsService::OnNewWindowBrowserWindowFirstPresentation(
    waap::NewWindowCreationSource source,
    bool with_existing_window,
    base::TimeTicks start_time,
    base::TimeTicks paint_time) {
  if (start_time.is_null() || paint_time.is_null() ||
      source == waap::NewWindowCreationSource::kUnknown) {
    return;
  }

  RecordNewWindowPaintMetric("BrowserWindow.FirstPaint.FromConstructor2",
                             source, with_existing_window, start_time,
                             paint_time);
}

void WaapUIMetricsService::OnNewWindowReloadButtonFirstPaint(
    waap::NewWindowCreationSource source,
    bool with_existing_window,
    base::TimeTicks start_time,
    base::TimeTicks paint_time) {
  if (start_time.is_null() || paint_time.is_null() ||
      source == waap::NewWindowCreationSource::kUnknown) {
    return;
  }

  RecordNewWindowPaintMetric("ReloadButton.FirstPaint.FromConstructor2", source,
                             with_existing_window, start_time, paint_time);
}

void WaapUIMetricsService::OnNewWindowReloadButtonFirstContentfulPaint(
    waap::NewWindowCreationSource source,
    bool with_existing_window,
    base::TimeTicks start_time,
    base::TimeTicks paint_time) {
  if (start_time.is_null() || paint_time.is_null() ||
      source == waap::NewWindowCreationSource::kUnknown) {
    return;
  }

  RecordNewWindowPaintMetric(
      "ReloadButton.FirstContentfulPaint.FromConstructor2", source,
      with_existing_window, start_time, paint_time);
}

void WaapUIMetricsService::OnStartupBrowserWindowToReloadButtonFirstPaintGap(
    base::TimeTicks browser_window_paint_time,
    base::TimeTicks reload_button_paint_time) {
  RecordStartupPaintMetric("BrowserWindowToReloadButton.FirstPaintGap",
                           browser_window_paint_time, reload_button_paint_time);
}

void WaapUIMetricsService::OnNewWindowBrowserWindowToReloadButtonFirstPaintGap(
    waap::NewWindowCreationSource source,
    bool with_existing_window,
    base::TimeTicks browser_window_paint_time,
    base::TimeTicks reload_button_paint_time) {
  RecordNewWindowPaintMetric("BrowserWindowToReloadButton.FirstPaintGap2",
                             source, with_existing_window,
                             browser_window_paint_time,
                             reload_button_paint_time);
}

void WaapUIMetricsService::OnReloadButtonPaintedAtBrowserFirstPaint(
    bool reload_button_painted) {
  base::UmaHistogramBoolean(
      BuildReloadButtonHistogramName("PaintedAtBrowserFirstPaint",
                                     GetSurfaceSyncSuffix()),
      reload_button_painted);
}

void WaapUIMetricsService::
    OnReloadButtonPaintedWithin10SecondsAfterBrowserPaint(
        bool painted_within_10s) {
  base::UmaHistogramBoolean(
      BuildReloadButtonHistogramName("PaintedWithin10SecondsAfterBrowserPaint",
                                     GetSurfaceSyncSuffix()),
      painted_within_10s);
}

void WaapUIMetricsService::OnInitialWebUISurfaceSyncResult(
    waap::InitialWebUISurfaceSyncResult result) {
  base::UmaHistogramEnumeration(
      BuildReloadButtonHistogramName("SurfaceSync.Result"), result);
}

void WaapUIMetricsService::OnSurfaceSyncTimeToPaintAfterDeadline(
    base::TimeDelta delta) {
  RecordReloadButtonPaintDuration("SurfaceSync.TimeToPaintAfterDeadline",
                                  delta);
}

void WaapUIMetricsService::OnBrowserPaintToReloadButtonPaint(
    base::TimeDelta delta) {
  RecordReloadButtonPaintDuration("BrowserPaintToReloadButtonPaint", delta,
                                  GetSurfaceSyncSuffix());
}

void WaapUIMetricsService::OnReloadButtonBrowserWindowClosedBeforePaint(
    base::TimeDelta delta) {
  RecordReloadButtonPaintDuration("BrowserWindowClosedBeforePaint", delta,
                                  GetSurfaceSyncSuffix());
}

void WaapUIMetricsService::OnStartupBrowserWindowShowRequestedToFirstPaint(
    base::TimeTicks request_time,
    base::TimeTicks paint_time) {
  RecordStartupPaintMetric("BrowserWindow.ShowRequestedToFirstPaint",
                           request_time, paint_time);
}

void WaapUIMetricsService::OnNewWindowBrowserWindowShowRequestedToFirstPaint(
    waap::NewWindowCreationSource source,
    bool with_existing_window,
    base::TimeTicks request_time,
    base::TimeTicks paint_time) {
  RecordNewWindowPaintMetric(
        "BrowserWindow.ShowRequestedToFirstPaint2", source,
        with_existing_window, request_time, paint_time);
}

void WaapUIMetricsService::OnStartupBrowserWindowClosedBeforeFirstPaint(
    base::TimeTicks request_time,
    base::TimeTicks close_time) {
  RecordStartupPaintMetric("BrowserWindow.ClosedBeforeFirstPaint", request_time,
                           close_time);
}

void WaapUIMetricsService::OnNewWindowBrowserWindowClosedBeforeFirstPaint(
    waap::NewWindowCreationSource source,
    bool with_existing_window,
    base::TimeTicks start_time,
    base::TimeTicks close_time) {
  RecordNewWindowPaintMetric("BrowserWindow.ClosedBeforeFirstPaint2", source,
                             with_existing_window, start_time, close_time);
}


void WaapUIMetricsService::OnReloadButtonInput(
    WaapUIMetricsRecorder::ReloadButtonInputType input_type) {
  auto name = BuildReloadButtonHistogramName("InputCount");
  base::UmaHistogramEnumeration(name, input_type);
}

void WaapUIMetricsService::RecordReloadButtonInteractionToReload(
    base::TimeTicks interaction_ticks,
    base::TimeTicks execution_ticks,
    WaapUIMetricsRecorder::ReloadButtonInputType input_type) {
  const base::TimeDelta duration =
      std::max(base::TimeDelta(), execution_ticks - interaction_ticks);
  const std::string name = BuildReloadButtonHistogramName(
      "InteractionToReload", ReloadButtonInputTypeToString(input_type));
  base::UmaHistogramCustomTimes(name, duration, base::Milliseconds(1),
                                base::Seconds(10), 100);

  const std::string aggregated_name =
      BuildReloadButtonHistogramName("InteractionToReload");
  base::UmaHistogramCustomTimes(aggregated_name, duration,
                                base::Milliseconds(1), base::Seconds(10), 100);
}
