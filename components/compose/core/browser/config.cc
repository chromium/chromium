// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/config.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/segmentation_platform/public/features.h"

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

  is_nudge_shown_at_cursor =
      base::FeatureList::IsEnabled(features::kEnableComposeNudgeAtCursor);

  saved_state_nudge_enabled =
      base::FeatureList::IsEnabled(features::kEnableComposeSavedStateNudge);

  proactive_nudge_enabled =
      base::FeatureList::IsEnabled(features::kEnableComposeProactiveNudge);

  proactive_nudge_compact_ui = base::GetFieldTrialParamByFeatureAsBool(
      features::kEnableComposeProactiveNudge, "proactive_nudge_compact_ui",
      proactive_nudge_compact_ui);

  proactive_nudge_show_probability = base::GetFieldTrialParamByFeatureAsDouble(
      features::kEnableComposeProactiveNudge,
      "proactive_nudge_show_probability", proactive_nudge_show_probability);

  proactive_nudge_force_show_probability =
      base::GetFieldTrialParamByFeatureAsDouble(
          features::kEnableComposeProactiveNudge,
          "proactive_nudge_force_show_probability",
          proactive_nudge_force_show_probability);

  proactive_nudge_always_collect_training_data =
      base::GetFieldTrialParamByFeatureAsBool(
          features::kEnableComposeProactiveNudge,
          "proactive_nudge_always_collect_training_data",
          proactive_nudge_always_collect_training_data);

  // Note: Pre M129 the feature param was |proactive_nudge_delay_milliseconds|
  // and was used for both the focus delay and after input delay. The default
  // value of the old parameter was set to 1 second https://crrev.com/c/5672112
  // which landed in M128. Before that it was 3 seconds.
  proactive_nudge_focus_delay =
      base::Milliseconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kEnableComposeProactiveNudge,
          "proactive_nudge_focus_delay_milliseconds",
          proactive_nudge_focus_delay.InMilliseconds()));

  proactive_nudge_text_settled_delay =
      base::Milliseconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kEnableComposeProactiveNudge,
          "proactive_nudge_text_settled_delay_milliseconds",
          proactive_nudge_text_settled_delay.InMilliseconds()));

  proactive_nudge_text_change_count = base::GetFieldTrialParamByFeatureAsInt(
      features::kEnableComposeProactiveNudge,
      "proactive_nudge_text_change_count", proactive_nudge_text_change_count);

  selection_nudge_enabled =
      base::FeatureList::IsEnabled(features::kEnableComposeSelectionNudge);

  selection_nudge_length =
      base::saturated_cast<unsigned int>(GetFieldTrialParamByFeatureAsInt(
          features::kEnableComposeSelectionNudge, "selection_nudge_length",
          selection_nudge_length));

  selection_nudge_delay =
      base::Milliseconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kEnableComposeSelectionNudge,
          "selection_nudge_delay_milliseconds",
          selection_nudge_delay.InMilliseconds()));

  selection_nudge_once_per_focus = base::GetFieldTrialParamByFeatureAsBool(
      features::kEnableComposeSelectionNudge, "selection_nudge_once_per_focus",
      selection_nudge_once_per_focus);

  nudge_field_change_event_max = base::GetFieldTrialParamByFeatureAsInt(
      features::kEnableComposeProactiveNudge, "nudge_field_change_event_max",
      nudge_field_change_event_max);

  proactive_nudge_segmentation = base::FeatureList::IsEnabled(
      segmentation_platform::features::kSegmentationPlatformComposePromotion);

  proactive_nudge_field_per_navigation =
      base::GetFieldTrialParamByFeatureAsBool(
          features::kEnableComposeProactiveNudge,
          "proactive_nudge_field_per_navigation",
          proactive_nudge_field_per_navigation);

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

  // The "enabled_countries" field trial param must contain a list of lowercase
  // country codes, following the format described in the documentation for the
  // variations::VariationsService::GetStoredPermanentCountry method. Commas,
  // spaces, tabs, new lines, single and double quotes are all treated as
  // separators and then discarded. A resulting empty list will be ignored in
  // favor of the default launched countries list.
  // To enable for any and all countries, set it to have a single "*" element.
  std::string enabled_countries_str = base::GetFieldTrialParamValueByFeature(
      features::kEnableCompose, "enabled_countries");
  std::vector<std::string> enabled_countries_from_finch =
      base::SplitString(enabled_countries_str, ", \t\n'\"",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (!enabled_countries_from_finch.empty()) {
    enabled_countries = enabled_countries_from_finch;
  }

  session_max_allowed_lifetime =
      base::Minutes(base::GetFieldTrialParamByFeatureAsInt(
          features::kEnableCompose, "session_max_allowed_lifetime_minutes",
          session_max_allowed_lifetime.InMinutes()));
}

Config::Config(const Config& other) = default;
Config::~Config() = default;

}  // namespace compose
