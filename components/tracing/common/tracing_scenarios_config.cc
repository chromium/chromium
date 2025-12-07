// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/tracing_scenarios_config.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "components/tracing/common/tracing_switches.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/histogram_samples.gen.h"
#include "third_party/perfetto/protos/perfetto/config/data_source_config.gen.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.gen.h"
#include "third_party/perfetto/protos/perfetto/config/track_event/track_event_config.gen.h"
#include "third_party/snappy/src/snappy.h"

namespace tracing {

BASE_FEATURE(kTracingTriggers, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFieldTracing, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPresetTracing, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kFieldTracingAnonymized,
                   &kFieldTracing,
                   "anonymized",
                   true);
BASE_FEATURE_PARAM(bool,
                   kFieldTracingForceUploads,
                   &kFieldTracing,
                   "force_uploads",
                   false);
BASE_FEATURE_PARAM(size_t,
                   kFieldTracingUploadLimitKb,
                   &kFieldTracing,
                   "upload_limit_kb",
                   0);
BASE_FEATURE_PARAM(bool,
                   kStartupFieldTracing,
                   &kFieldTracing,
                   "startup",
                   false);

namespace {

const base::FeatureParam<std::string> kTracingTriggerRulesConfig{
    &kTracingTriggers, "config", ""};
const base::FeatureParam<std::string> kFieldTracingConfig{&kFieldTracing,
                                                          "config", ""};
const base::FeatureParam<std::string> kPresetTracingConfig{&kPresetTracing,
                                                           "config", ""};

constexpr const char* kBrowserCategoriesList[] = {
    "accessibility",
    "base.power",
    "base",
    "benchmark",
    "blink_gc",
    "blink",
    "browser",
    "cc",
    "chromeos",
    "content",
    "device",
    "disabled-by-default-cpu_profiler",
    "disabled-by-default-power",
    "disabled-by-default-system_metrics",
    "disabled-by-default-user_action_samples",
    "disabled-by-default-v8.gc",
    "disk_cache",
    "dwrite",
    "extensions",
    "fledge",
    "fonts",
    "gpu",
    "IndexedDB",
    "interactions",
    "ipc",
    "latency",
    "latencyInfo",
    "loading",
    "memory",
    "mojom.flow",
    "mojom",
    "navigation",
    "omnibox",
    "passwords",
    "performance_manager.cpu_metrics",
    "performance_manager.graph",
    "performance_scenarios",
    "renderer_host",
    "renderer",
    "safe_browsing",
    "ServiceWorker",
    "shutdown",
    "sql",
    "startup",
    "sync",
    "toplevel.flow",
    "toplevel",
    "tracing.background",
    "ui",
    "v8.memory",
    "v8.wasm",
    "v8",
};

constexpr const char* kHistogramSamplesFilterList[] = {
    "Blink.Responsiveness.UserInteraction.MaxEventDuration.AllTypes",
    "PageLoad.InteractiveTiming.InputDelay3",
    "Event.Latency.OS2.MOUSE_WHEEL",
    "Event.Latency.OS2.MOUSE_PRESSED",
    "Event.Latency.OS2.KEY_PRESSED",
    ("Blink.Responsiveness.PerAnimationFrame.EventCreationToPresentationTime"
     ".All"),
    "Omnibox.CharTypedToRepaintLatency",
    "Browser.Tabs.TotalSwitchDuration3",
    "Browser.MainThreadsCongestion",
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint",
    "HangWatcher.IsThreadHung.Any",
    "Startup.FirstWebContents.NonEmptyPaint3",
    "Graphics.Smoothness.PercentDroppedFrames3.AllSequences",
    "Event.ScrollJank.DelayedFramesPercentage.FixedWindow"};

void SetHistogramTriggerRule(perfetto::protos::gen::TriggerRule* rule,
                             const char* histogram_name,
                             std::optional<int> min_value = std::nullopt,
                             std::optional<int> max_value = std::nullopt) {
  auto* histogram = rule->mutable_histogram();
  histogram->set_histogram_name(histogram_name);
  if (min_value) {
    histogram->set_min_value(*min_value);
  }
  if (max_value) {
    histogram->set_max_value(*max_value);
  }
}

perfetto::protos::gen::ChromeFieldTracingConfig
CreateDefaultPresetTracingScenariosConfig() {
  perfetto::protos::gen::ChromeFieldTracingConfig config;
  auto* scenario = config.add_scenarios();
  scenario->set_scenario_name("AlwaysOnScenario");
  scenario->set_scenario_description(
      "This scenario runs at all time in circular buffer mode, and captures a "
      "trace snapshot when a user report is created, e.g. with "
      "'Help > Report an Issue'.");
  scenario->add_start_rules()->set_manual_trigger_name("startup");
  scenario->add_start_rules()->set_delay_ms(1);
  scenario->add_start_rules()->set_manual_trigger_name("incognito-end");
  scenario->add_stop_rules()->set_manual_trigger_name("incognito-start");
  auto* nested_scenario = scenario->add_nested_scenarios();
  nested_scenario->set_scenario_name("AlwaysOnScenario.Snapshots");
  nested_scenario->add_start_rules()->set_delay_ms(1);

  SetHistogramTriggerRule(nested_scenario->add_upload_rules(),
                          "Feedback.RequestSource");
  // Renderer crash count - Extension renderer crash count
  SetHistogramTriggerRule(nested_scenario->add_upload_rules(),
                          "Stability.Counts2", 3, 5);
  // Renderer failed launch count - Extension renderer failed launch count
  SetHistogramTriggerRule(nested_scenario->add_upload_rules(),
                          "Stability.Counts2", 24, 25);
  // GPU process crash count - Utility process crash count
  SetHistogramTriggerRule(nested_scenario->add_upload_rules(),
                          "Stability.Counts2", 31, 32);

  auto* trace_config = scenario->mutable_trace_config();
  {
    auto* buffer = trace_config->add_buffers();
    buffer->set_size_kb(64 * 1024);
    buffer->set_fill_policy(
        perfetto::protos::gen::TraceConfig::BufferConfig::RING_BUFFER);
  }
  {
    auto* buffer = trace_config->add_buffers();
    buffer->set_size_kb(256);
    buffer->set_fill_policy(
        perfetto::protos::gen::TraceConfig::BufferConfig::DISCARD);
  }
  {
    auto* ds = trace_config->add_data_sources()->mutable_config();
    ds->set_name("org.chromium.trace_metadata2");
    ds->set_target_buffer(1);
  }
  {
    auto* ds = trace_config->add_data_sources()->mutable_config();
    ds->set_name("org.chromium.background_scenario_metadata");
    ds->set_target_buffer(1);
  }
  trace_config->add_data_sources()->mutable_config()->set_name(
      "org.chromium.triggers");
  trace_config->add_data_sources()->mutable_config()->set_name(
      "org.chromium.system_metrics");
  trace_config->add_data_sources()->mutable_config()->set_name(
      "org.chromium.sampler_profiler");
  {
    auto* ds = trace_config->add_data_sources()->mutable_config();
    ds->set_name("org.chromium.histogram_sample");
    perfetto::protos::gen::ChromiumHistogramSamplesConfig histogram_config;
    for (auto* histogram : kHistogramSamplesFilterList) {
      histogram_config.add_histograms()->set_histogram_name(histogram);
    }
    ds->set_chromium_histogram_samples_raw(
        histogram_config.SerializeAsString());
  }
  {
    auto* ds = trace_config->add_data_sources()->mutable_config();
    ds->set_name("track_event");
    perfetto::protos::gen::TrackEventConfig track_event_config;
    track_event_config.add_disabled_categories("*");
    for (auto* category : kBrowserCategoriesList) {
      track_event_config.add_enabled_categories(category);
    }
    ds->set_track_event_config_raw(track_event_config.SerializeAsString());
  }
  return config;
}

}  // namespace

std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
GetPresetTracingScenariosConfig() {
  if (!base::FeatureList::IsEnabled(kPresetTracing)) {
    return CreateDefaultPresetTracingScenariosConfig();
  }
  return ParseEncodedTracingScenariosConfig(kPresetTracingConfig.Get());
}

std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
GetFieldTracingScenariosConfig() {
  if (!base::FeatureList::IsEnabled(kFieldTracing)) {
    return std::nullopt;
  }
  return ParseEncodedTracingScenariosConfig(kFieldTracingConfig.Get());
}

std::optional<perfetto::protos::gen::TracingTriggerRulesConfig>
GetTracingTriggerRulesConfig() {
  if (!base::FeatureList::IsEnabled(kTracingTriggers)) {
    return std::nullopt;
  }
  std::string serialized_config;
  if (!base::Base64Decode(kTracingTriggerRulesConfig.Get(),
                          &serialized_config)) {
    return std::nullopt;
  }

  // `serialized_config` may optionally be compressed.
  std::string decompressed_config;
  if (snappy::Uncompress(serialized_config.data(), serialized_config.size(),
                         &decompressed_config)) {
    serialized_config = std::move(decompressed_config);
  }
  perfetto::protos::gen::TracingTriggerRulesConfig config;
  if (config.ParseFromString(serialized_config)) {
    return config;
  }
  return std::nullopt;
}

std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
ParseSerializedTracingScenariosConfig(
    const base::span<const uint8_t>& config_bytes) {
  perfetto::protos::gen::ChromeFieldTracingConfig config;
  if (config_bytes.empty()) {
    return std::nullopt;
  }
  if (config.ParseFromArray(config_bytes.data(), config_bytes.size())) {
    return config;
  }
  return std::nullopt;
}

std::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
ParseEncodedTracingScenariosConfig(const std::string& config_string) {
  std::string serialized_config;
  if (!base::Base64Decode(config_string, &serialized_config,
                          base::Base64DecodePolicy::kForgiving)) {
    return std::nullopt;
  }

  // `serialized_config` may optionally be compressed.
  std::string decompressed_config;
  if (!snappy::Uncompress(serialized_config.data(), serialized_config.size(),
                          &decompressed_config)) {
    return ParseSerializedTracingScenariosConfig(
        base::as_byte_span(serialized_config));
  }

  return ParseSerializedTracingScenariosConfig(
      base::as_byte_span(decompressed_config));
}

bool IsBackgroundTracingEnabledFromCommandLine() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableBackgroundTracing);
}

}  // namespace tracing
