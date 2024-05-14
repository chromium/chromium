// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/debug/metrics_internals_utils.h"

#include <string_view>

#include "components/metrics/metrics_pref_names.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/service/variations_service.h"

namespace metrics {
namespace {

std::string ChannelToString(variations::Study::Channel channel) {
  switch (channel) {
    case variations::Study::UNKNOWN:
      return "Unknown";
    case variations::Study::CANARY:
      return "Canary";
    case variations::Study::DEV:
      return "Dev";
    case variations::Study::BETA:
      return "Beta";
    case variations::Study::STABLE:
      return "Stable";
  }
  NOTREACHED_IN_MIGRATION();
}

std::string PlatformToString(variations::Study::Platform platform) {
  switch (platform) {
    case variations::Study::PLATFORM_WINDOWS:
      return "Windows";
    case variations::Study::PLATFORM_MAC:
      return "Mac";
    case variations::Study::PLATFORM_LINUX:
      return "Linux";
    case variations::Study::PLATFORM_CHROMEOS:
      return "ChromeOS";
    case variations::Study::PLATFORM_ANDROID:
      return "Android";
    case variations::Study::PLATFORM_IOS:
      return "iOS";
    case variations::Study::PLATFORM_ANDROID_WEBVIEW:
      return "WebView";
    case variations::Study::PLATFORM_FUCHSIA:
      return "Fuchsia";
    case variations::Study::PLATFORM_ANDROID_WEBLAYER:
      return "WebLayer";
    case variations::Study::PLATFORM_CHROMEOS_LACROS:
      return "ChromeOS Lacros";
  }
  NOTREACHED_IN_MIGRATION();
}

std::string CpuArchitectureToString(
    variations::Study::CpuArchitecture cpu_architecture) {
  switch (cpu_architecture) {
    case variations::Study::X86_64:
      return "x86_64";
    case variations::Study::ARM64:
      return "arm64";
    case variations::Study::X86_32:
      return "x86_32";
    case variations::Study::ARM32:
      return "arm32";
    case variations::Study::TRANSLATED_X86_64:
      return "translated_x86_64";
  }
  NOTREACHED_IN_MIGRATION();
}

std::string FormFactorToString(variations::Study::FormFactor form_factor) {
  switch (form_factor) {
    case variations::Study::DESKTOP:
      return "Desktop";
    case variations::Study::PHONE:
      return "Phone";
    case variations::Study::TABLET:
      return "Tablet";
    case variations::Study::KIOSK:
      return "Kiosk";
    case variations::Study::MEET_DEVICE:
      return "Meet Device";
    case variations::Study::TV:
      return "TV";
    case variations::Study::AUTOMOTIVE:
      return "Automotive";
    case variations::Study::FOLDABLE:
      return "Foldable";
  }
  NOTREACHED_IN_MIGRATION();
}

std::string BoolToString(bool val) {
  return val ? "Yes" : "No";
}

base::Value::Dict CreateKeyValueDict(std::string_view key,
                                     std::string_view value) {
  base::Value::Dict dict;
  dict.Set("key", key);
  dict.Set("value", value);
  return dict;
}

}  // namespace

base::Value::List GetUmaSummary(MetricsService* metrics_service) {
  base::Value::List list;
  list.Append(CreateKeyValueDict("Client ID", metrics_service->GetClientId()));
  // TODO(crbug.com/40238818): Add the server-side client ID.
  list.Append(CreateKeyValueDict(
      "Metrics Reporting Enabled",
      BoolToString(metrics_service->IsMetricsReportingEnabled())));
  list.Append(
      CreateKeyValueDict("Currently Recording",
                         BoolToString(metrics_service->recording_active())));
  list.Append(
      CreateKeyValueDict("Currently Reporting",
                         BoolToString(metrics_service->reporting_active())));
  return list;
}

base::Value::List GetVariationsSummary(
    metrics_services_manager::MetricsServicesManager* metrics_service_manager) {
  base::Value::List list;
  std::unique_ptr<variations::ClientFilterableState> state =
      metrics_service_manager->GetVariationsService()
          ->GetClientFilterableStateForVersion();
  list.Append(CreateKeyValueDict("Channel", ChannelToString(state->channel)));
  list.Append(CreateKeyValueDict("Version", state->version.GetString()));
  list.Append(
      CreateKeyValueDict("Platform", PlatformToString(state->platform)));
  list.Append(CreateKeyValueDict("OS Version", state->os_version.GetString()));
  list.Append(CreateKeyValueDict(
      "CPU Architecture", CpuArchitectureToString(state->cpu_architecture)));
  list.Append(CreateKeyValueDict("Hardware Class", state->hardware_class));
  list.Append(CreateKeyValueDict("Form Factor",
                                 FormFactorToString(state->form_factor)));
  list.Append(CreateKeyValueDict("Low End Device",
                                 BoolToString(state->is_low_end_device)));
  list.Append(CreateKeyValueDict("Country (Session Consistency)",
                                 state->session_consistency_country));
  list.Append(CreateKeyValueDict("Country (Permanent Consistency)",
                                 state->permanent_consistency_country));
  list.Append(CreateKeyValueDict("Locale", state->locale));
  list.Append(
      CreateKeyValueDict("Enterprise", BoolToString(state->IsEnterprise())));
  return list;
}

}  // namespace metrics
