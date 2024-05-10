// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/origin_trials/component_updater_utils.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/embedder_support/origin_trials/origin_trials_settings_storage.h"
#include "components/embedder_support/origin_trials/pref_names.h"
#include "components/embedder_support/switches.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr char kManifestPublicKeyPath[] = "origin-trials.public-key";
constexpr char kManifestDisabledFeaturesPath[] =
    "origin-trials.disabled-features";
constexpr char kManifestDisabledTokenSignaturesPath[] =
    "origin-trials.disabled-tokens.signatures";

}  // namespace

namespace embedder_support {

void ReadOriginTrialsConfigAndPopulateLocalState(PrefService* local_state,
                                                 base::Value::Dict manifest) {
  DCHECK(local_state);

  if (std::string* override_public_key =
          manifest.FindStringByDottedPath(kManifestPublicKeyPath)) {
    local_state->SetString(prefs::kOriginTrialPublicKey, *override_public_key);
  } else {
    local_state->ClearPref(prefs::kOriginTrialPublicKey);
  }

  base::Value::List* override_disabled_feature_list =
      manifest.FindListByDottedPath(kManifestDisabledFeaturesPath);
  if (override_disabled_feature_list &&
      !override_disabled_feature_list->empty()) {
    local_state->SetList(prefs::kOriginTrialDisabledFeatures,
                         std::move(*override_disabled_feature_list));
  } else {
    local_state->ClearPref(prefs::kOriginTrialDisabledFeatures);
  }

  base::Value::List* disabled_tokens_list =
      manifest.FindListByDottedPath(kManifestDisabledTokenSignaturesPath);
  if (disabled_tokens_list && !disabled_tokens_list->empty()) {
    local_state->SetList(prefs::kOriginTrialDisabledTokens,
                         std::move(*disabled_tokens_list));
  } else {
    local_state->ClearPref(prefs::kOriginTrialDisabledTokens);
  }
}

void SetupOriginTrialsCommandLineAndSettings(
    PrefService* local_state,
    OriginTrialsSettingsStorage* settings_storage) {
  CHECK(settings_storage);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kOriginTrialPublicKey)) {
    std::string new_public_key =
        local_state->GetString(prefs::kOriginTrialPublicKey);
    if (!new_public_key.empty()) {
      command_line->AppendSwitchASCII(
          kOriginTrialPublicKey,
          local_state->GetString(prefs::kOriginTrialPublicKey));
    }
  }
  if (!command_line->HasSwitch(kOriginTrialDisabledFeatures)) {
    const base::Value::List& override_disabled_feature_list =
        local_state->GetList(prefs::kOriginTrialDisabledFeatures);
    std::vector<std::string_view> disabled_features;
    for (const auto& item : override_disabled_feature_list) {
      if (item.is_string())
        disabled_features.push_back(item.GetString());
    }
    if (!disabled_features.empty()) {
      const std::string override_disabled_features =
          base::JoinString(disabled_features, "|");
      command_line->AppendSwitchASCII(kOriginTrialDisabledFeatures,
                                      override_disabled_features);
    }
  }
  // TODO(crbug.com/40770598): Should revisit if we want to continue allowing
  // users to override the disabled tokens list via a CLI flag or remove that
  // functionality and populate the settings only from the PrefService.
  if (!command_line->HasSwitch(kOriginTrialDisabledTokens)) {
    const base::Value::List& disabled_token_list =
        local_state->GetList(prefs::kOriginTrialDisabledTokens);
    settings_storage->PopulateSettings(std::move(disabled_token_list));
  } else {
    const std::string& disabled_token_value =
        command_line->GetSwitchValueASCII(kOriginTrialDisabledTokens);
    const std::vector<std::string> tokens =
        base::SplitString(disabled_token_value, "|", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    base::Value::List disabled_token_list;
    for (auto& ascii_token : tokens) {
      disabled_token_list.Append(std::move(ascii_token));
    }
    settings_storage->PopulateSettings(std::move(disabled_token_list));
  }
}

}  // namespace embedder_support
