// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/origin_trials/component_updater_utils.h"

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

void ReadOriginTrialsConfigAndPopulateLocalState(PrefService* local_state,
                                                 base::Value manifest) {
  DCHECK(local_state);

  if (std::string* override_public_key =
          manifest.FindStringPath(kManifestPublicKeyPath)) {
    local_state->Set(prefs::kOriginTrialPublicKey,
                     base::Value(*override_public_key));
  } else {
    local_state->ClearPref(prefs::kOriginTrialPublicKey);
  }

  // TODO(crbug.com/1187062): Modernize use of base::ListValue once
  // ListPrefUpdate is converted.
  base::ListValue* override_disabled_feature_list = nullptr;
  if (base::Value* raw_override_disabled_feature_list =
          manifest.FindListPath(kManifestDisabledFeaturesPath)) {
    raw_override_disabled_feature_list->GetAsList(
        &override_disabled_feature_list);
  }
  if (override_disabled_feature_list &&
      !override_disabled_feature_list->GetList().empty()) {
    ListPrefUpdate update(local_state, prefs::kOriginTrialDisabledFeatures);
    update->Swap(override_disabled_feature_list);
  } else {
    local_state->ClearPref(prefs::kOriginTrialDisabledFeatures);
  }

  // TODO(crbug.com/1187062): Modernize use of base::ListValue once
  // ListPrefUpdate is converted.
  base::ListValue* disabled_tokens_list = nullptr;
  if (base::Value* raw_disabled_tokens_list =
          manifest.FindListPath(kManifestDisabledTokenSignaturesPath)) {
    raw_disabled_tokens_list->GetAsList(&disabled_tokens_list);
  }
  if (disabled_tokens_list && !disabled_tokens_list->GetList().empty()) {
    ListPrefUpdate update(local_state, prefs::kOriginTrialDisabledTokens);
    update->Swap(disabled_tokens_list);
  } else {
    local_state->ClearPref(prefs::kOriginTrialDisabledTokens);
  }
}

}  // namespace embedder_support
