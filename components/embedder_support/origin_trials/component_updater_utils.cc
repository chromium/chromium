// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/origin_trials/component_updater_utils.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/values.h"
#include "components/embedder_support/origin_trials/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

constexpr char kManifestPublicKeyPath[] = "origin-trials.public-key";
constexpr char kManifestDisabledFeaturesPath[] =
    "origin-trials.disabled-features";
constexpr char kManifestDisabledTokenSignaturesPath[] =
    "origin-trials.disabled-tokens.signatures";

}  // namespace

namespace embedder_support {

void ReadOriginTrialsConfigAndPopulateLocalState(
    PrefService* local_state,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DCHECK(local_state);

  std::string override_public_key;
  if (manifest->GetString(kManifestPublicKeyPath, &override_public_key)) {
    local_state->Set(prefs::kOriginTrialPublicKey,
                     base::Value(override_public_key));
  } else {
    local_state->ClearPref(prefs::kOriginTrialPublicKey);
  }
  base::ListValue* override_disabled_feature_list = nullptr;
  const bool manifest_has_disabled_features = manifest->GetList(
      kManifestDisabledFeaturesPath, &override_disabled_feature_list);
  if (manifest_has_disabled_features &&
      !override_disabled_feature_list->empty()) {
    ListPrefUpdate update(local_state, prefs::kOriginTrialDisabledFeatures);
    update->Swap(override_disabled_feature_list);
  } else {
    local_state->ClearPref(prefs::kOriginTrialDisabledFeatures);
  }
  base::ListValue* disabled_tokens_list = nullptr;
  const bool manifest_has_disabled_tokens = manifest->GetList(
      kManifestDisabledTokenSignaturesPath, &disabled_tokens_list);
  if (manifest_has_disabled_tokens && !disabled_tokens_list->empty()) {
    ListPrefUpdate update(local_state, prefs::kOriginTrialDisabledTokens);
    update->Swap(disabled_tokens_list);
  } else {
    local_state->ClearPref(prefs::kOriginTrialDisabledTokens);
  }
}

}  // namespace embedder_support
