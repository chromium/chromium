// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_trial.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

// static
void DefaultBrowserPromptTrial::MaybeJoinDefaultBrowserPromptCohort() {
  PrefService *local_state = g_browser_process->local_state();
  if (!local_state) {
    return; // Can be null in unit tests;
  }

  std::string active_study_group =
      features::kDefaultBrowserPromptRefreshStudyGroup.Get();
  // If the study group isn't set, don't add the user to the cohort.
  if (active_study_group.empty()) {
    return;
  }

  local_state->SetString(prefs::kDefaultBrowserPromptRefreshStudyGroup,
                         active_study_group);
  DefaultBrowserPromptTrial::RegisterSyntheticFieldTrial(active_study_group);
}

// static
void DefaultBrowserPromptTrial::EnsureStickToDefaultBrowserPromptCohort() {
  PrefService *local_state = g_browser_process->local_state();
  if (!local_state) {
    return; // Can be null in unit tests;
  }

  auto enrolled_study_group =
      local_state->GetString(prefs::kDefaultBrowserPromptRefreshStudyGroup);
  if (enrolled_study_group.empty()) {
    // The user was not enrolled or exited the study at some point.
    return;
  }

  DefaultBrowserPromptTrial::RegisterSyntheticFieldTrial(enrolled_study_group);
}

// static
void DefaultBrowserPromptTrial::RegisterSyntheticFieldTrial(
    const std::string &group_name) {
  CHECK(!group_name.empty());

  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "DefaultBrowserPromptRefreshSynthetic", group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}
