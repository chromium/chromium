// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/config.h"

#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
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
      features::kEnableCompose, "input_min_words", input_min_words);

  input_max_words = base::GetFieldTrialParamByFeatureAsInt(
      features::kEnableCompose, "input_max_words", input_max_words);

  input_max_chars = base::GetFieldTrialParamByFeatureAsInt(
      features::kEnableCompose, "input_max_chars", input_max_chars);

  inner_text_max_bytes = base::GetFieldTrialParamByFeatureAsInt(
      features::kEnableCompose, "inner_text_max_bytes", inner_text_max_bytes);

  auto_submit_with_selection = base::GetFieldTrialParamByFeatureAsBool(
      features::kEnableCompose, "auto_submit_with_selection",
      auto_submit_with_selection);

  popup_with_saved_state = base::GetFieldTrialParamByFeatureAsBool(
      features::kEnableComposeNudge, "popup_with_saved_state",
      popup_with_saved_state);

  popup_with_no_saved_state = base::GetFieldTrialParamByFeatureAsBool(
      features::kEnableComposeNudge, "popup_with_no_saved_state",
      popup_with_no_saved_state);
}

Config::Config(const Config& other) = default;
Config::~Config() = default;

}  // namespace compose
