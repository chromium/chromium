// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/config.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/compose/core/browser/compose_features.h"

namespace compose {

namespace {

Config& GetMutableConfig() {
  static base::NoDestructor<Config> s_config;
  return *s_config;
}

}  // namespace

const Config& GetComposeConfig() {
  return GetMutableConfig();
}

Config& GetMutableConfigForTesting() {
  return GetMutableConfig();
}

void ResetConfigForTesting() {
  GetMutableConfig() = Config();
}

Config::Config() {
  input_min_words = base::GetFieldTrialParamByFeatureAsInt(
      features::kComposeInputParams, "min_words", input_min_words);

  input_max_words = base::GetFieldTrialParamByFeatureAsInt(
      features::kComposeInputParams, "max_words", input_max_words);

  input_max_chars = base::GetFieldTrialParamByFeatureAsInt(
      features::kComposeInputParams, "max_chars", input_max_chars);

  inner_text_max_bytes = base::GetFieldTrialParamByFeatureAsInt(
      features::kComposeInnerText, "inner_text_max_bytes",
      inner_text_max_bytes);

  trimmed_inner_text_max_chars = base::GetFieldTrialParamByFeatureAsInt(
      features::kComposeInnerText, "trimmed_inner_text_max_chars",
      trimmed_inner_text_max_chars);

  trimmed_inner_text_header_length = base::GetFieldTrialParamByFeatureAsInt(
      features::kComposeInnerText, "trimmed_inner_text_header_length",
      trimmed_inner_text_header_length);

  auto_submit_with_selection =
      base::FeatureList::IsEnabled(features::kComposeAutoSubmit);

  saved_state_nudge_enabled =
      base::FeatureList::IsEnabled(features::kEnableComposeSavedStateNudge);

  proactive_nudge_enabled =
      base::FeatureList::IsEnabled(features::kEnableComposeProactiveNudge);

  proactive_nudge_show_probability = base::GetFieldTrialParamByFeatureAsDouble(
      features::kEnableComposeProactiveNudge,
      "proactive_nudge_show_probability", proactive_nudge_show_probability);

  proactive_nudge_delay =
      base::Milliseconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kEnableComposeProactiveNudge,
          "proactive_nudge_delay_milliseconds",
          proactive_nudge_delay.InMilliseconds()));

  saved_state_timeout_milliseconds = base::GetFieldTrialParamByFeatureAsInt(
      features::kEnableComposeSavedStateNotification,
      "saved_state_timeout_milliseconds", saved_state_timeout_milliseconds);

  focus_lost_delay_milliseconds = base::GetFieldTrialParamByFeatureAsInt(
      features::kEnableComposeSavedStateNotification,
      "focus_lost_delay_milliseconds", focus_lost_delay_milliseconds);

  stay_in_window_bounds = base::GetFieldTrialParamByFeatureAsBool(
      features::kComposeUiParams, "stay_in_window_bounds",
      stay_in_window_bounds);

  positioning_strategy = static_cast<DialogFallbackPositioningStrategy>(
      base::GetFieldTrialParamByFeatureAsInt(
          features::kComposeUiParams, "positioning_strategy",
          base::to_underlying(positioning_strategy)));

  request_latency_timeout_seconds = base::GetFieldTrialParamByFeatureAsInt(
      features::kComposeRequestLatencyTimeout,
      "request_latency_timeout_seconds", request_latency_timeout_seconds);
}

Config::Config(const Config& other) = default;
Config::~Config() = default;

}  // namespace compose
