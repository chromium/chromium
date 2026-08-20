// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_SUGGESTIONS_CONSENT_STATE_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_SUGGESTIONS_CONSENT_STATE_H_

#include "components/optimization_guide/core/optimization_guide_prefs.h"

namespace multistep_filter {

// Represents the user's account state for multistep filter. Provides
// information about whether the user is signed in and eligible for model
// execution features.
// LINT.IfChange(AccountState)
struct AccountState {
  bool is_signed_in = false;
  bool can_use_model_execution_features = false;

  // Returns true if the user's account is eligible for multistep filter.
  bool IsEligible() const {
    return is_signed_in && can_use_model_execution_features;
  }
};
// LINT.ThenChange(//chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals.mojom:AccountStatus)

// Represents the user's consent state for multistep filter. Provides
// information about whether the user has consented to the features required
// for multistep filter to be enabled.
// LINT.IfChange(ConsentState)
struct ConsentState {
  bool is_msbb_enabled = false;
  bool is_history_sync_enabled = false;

  // Returns true if the user has consented to both MSBB and history sync.
  bool IsFullyConsented() const {
    return is_msbb_enabled && is_history_sync_enabled;
  }
};
// LINT.ThenChange(//chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals.mojom:ConsentStatus)

// Represents the user's suggestions policy state for multistep filter.
// The values of this enum correspond to the integer settings values defined
// in `contextual_cueing::ChromeSuggestionsSettingsValue` inside
// the chrome-level header `chrome/browser/contextual_cueing/prefs.h`.
// We define it here because this component cannot import chrome-level headers.
enum class SuggestionsPolicyState {
  kEnabled = 0,
  kDisabled = 1,
};

// Represents the user's settings state for multistep filter. Provides
// information about whether the user has enabled smart suggestions via settings
// and whether it is disabled by enterprise policy.
// LINT.IfChange(SettingsState)
struct SettingsState {
  optimization_guide::prefs::FeatureOptInState opt_in_state =
      optimization_guide::prefs::FeatureOptInState::kNotInitialized;
  SuggestionsPolicyState policy_state = SuggestionsPolicyState::kEnabled;

  // Returns true if the user has enabled smart suggestions via settings
  // and it is not disabled by enterprise policy.
  bool IsSmartSuggestionsEnabled() const {
    if (opt_in_state ==
        optimization_guide::prefs::FeatureOptInState::kDisabled) {
      return false;
    }
    if (policy_state == SuggestionsPolicyState::kDisabled) {
      return false;
    }
    return true;
  }
};
// LINT.ThenChange(//chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals.mojom:SettingsStatus)

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_SUGGESTIONS_CONSENT_STATE_H_
