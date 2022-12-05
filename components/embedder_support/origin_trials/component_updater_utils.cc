// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/origin_trials/component_updater_utils.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
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

void SetupOriginTrialsCommandLine(PrefService* local_state) {
  // TODO(crbug.com/1211739): Temporary workaround to prevent an overly large
  // config from crashing by exceeding command-line length limits. Set the limit
  // to 1KB, which is far less than the known limits:
  //  - Linux: kZygoteMaxMessageLength = 12288;
  // This will still allow for critical updates to the public key or disabled
  // features, but the disabled token list will be ignored.
  const size_t kMaxAppendLength = 1024;
  size_t appended_length = 0;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kOriginTrialPublicKey)) {
    std::string new_public_key =
        local_state->GetString(prefs::kOriginTrialPublicKey);
    if (!new_public_key.empty()) {
      command_line->AppendSwitchASCII(
          kOriginTrialPublicKey,
          local_state->GetString(prefs::kOriginTrialPublicKey));

      // Public key is 32 bytes
      appended_length += 32;
    }
  }
  if (!command_line->HasSwitch(kOriginTrialDisabledFeatures)) {
    const base::Value::List& override_disabled_feature_list =
        local_state->GetList(prefs::kOriginTrialDisabledFeatures);
    std::vector<base::StringPiece> disabled_features;
    for (const auto& item : override_disabled_feature_list) {
      if (item.is_string())
        disabled_features.push_back(item.GetString());
    }
    if (!disabled_features.empty()) {
      const std::string override_disabled_features =
          base::JoinString(disabled_features, "|");
      command_line->AppendSwitchASCII(kOriginTrialDisabledFeatures,
                                      override_disabled_features);
      appended_length += override_disabled_features.length();
    }
  }
  if (!command_line->HasSwitch(kOriginTrialDisabledTokens)) {
    const base::Value::List& disabled_token_list =
        local_state->GetList(prefs::kOriginTrialDisabledTokens);
    std::vector<base::StringPiece> disabled_tokens;
    for (const auto& item : disabled_token_list) {
      if (item.is_string())
        disabled_tokens.push_back(item.GetString());
    }
    if (!disabled_tokens.empty()) {
      const std::string disabled_token_switch =
          base::JoinString(disabled_tokens, "|");
      // Do not append the disabled token list if will exceed a reasonable
      // length. See above.
      if (appended_length + disabled_token_switch.length() <=
          kMaxAppendLength) {
        command_line->AppendSwitchASCII(kOriginTrialDisabledTokens,
                                        disabled_token_switch);
      }
    }
  }
}

}  // namespace embedder_support
