// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_SUGGESTION_USER_DECISION_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_SUGGESTION_USER_DECISION_H_

namespace multistep_filter {

// LINT.IfChange(SuggestionUserDecision)
// The user's decision upon interacting with a Multistep Filter suggestion.
// These values are persisted in UMA logs. Values should not be
// reused/renumbered.
enum class SuggestionUserDecision {
  kAccepted = 0,
  kIgnored = 1,
  kDismissed = 2,
  kSettingsOpened = 3,
  kMaxValue = kSettingsOpened,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/multistep_filter/enums.xml:SuggestionUserDecision)

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_DATA_MODELS_SUGGESTION_USER_DECISION_H_
