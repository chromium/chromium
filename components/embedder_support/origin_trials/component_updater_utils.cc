// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/origin_trials/component_updater_utils.h"

#include <string>

#include "base/check.h"
#include "base/values.h"
#include "components/embedder_support/origin_trials/pref_names.h"
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
                                                 base::Value manifest) {
  DCHECK(local_state);

  base::Value::Dict& manifest_dict = manifest.GetDict();

  if (std::string* override_public_key =
          manifest_dict.FindStringByDottedPath(kManifestPublicKeyPath)) {
    local_state->Set(prefs::kOriginTrialPublicKey,
                     base::Value(*override_public_key));
  } else {
    local_state->ClearPref(prefs::kOriginTrialPublicKey);
  }

  base::Value::List* override_disabled_feature_list =
      manifest_dict.FindListByDottedPath(kManifestDisabledFeaturesPath);
  if (override_disabled_feature_list &&
      !override_disabled_feature_list->empty()) {
    local_state->SetList(prefs::kOriginTrialDisabledFeatures,
                         std::move(*override_disabled_feature_list));
  } else {
    local_state->ClearPref(prefs::kOriginTrialDisabledFeatures);
  }

  base::Value::List* disabled_tokens_list =
      manifest_dict.FindListByDottedPath(kManifestDisabledTokenSignaturesPath);
  if (disabled_tokens_list && !disabled_tokens_list->empty()) {
    local_state->SetList(prefs::kOriginTrialDisabledTokens,
                         std::move(*disabled_tokens_list));
  } else {
    local_state->ClearPref(prefs::kOriginTrialDisabledTokens);
  }
}

}  // namespace embedder_support
