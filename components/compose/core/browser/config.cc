// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/config.h"

#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "components/compose/core/browser/compose_features.h"

namespace compose {
namespace {
Config g_config;

// Override any parameters that may be provided by Finch.
void OverrideFieldTrialParams(Config& config) {
  config.input_min_words = base::GetFieldTrialParamByFeatureAsInt(
      features::kEnableCompose, "input_min_words", config.input_min_words);

  config.input_max_words = base::GetFieldTrialParamByFeatureAsInt(
      features::kEnableCompose, "input_max_words", config.input_max_words);

  config.input_max_chars = base::GetFieldTrialParamByFeatureAsInt(
      features::kEnableCompose, "input_max_chars", config.input_max_chars);
}

}  // namespace

const Config& GetComposeConfig() {
  static base::NoDestructor<Config> s_config;
  OverrideFieldTrialParams(*s_config);
  return *s_config;
}

void SetComposeConfigForTesting(const Config& config) {
  const_cast<Config&>(GetComposeConfig()) = config;
}

void OverrideFieldTrialParamsForTesting() {
  OverrideFieldTrialParams(const_cast<Config&>(GetComposeConfig()));
}

Config::Config() = default;
Config::Config(const Config& other) = default;
Config::~Config() = default;

}  // namespace compose
