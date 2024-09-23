// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace {
bool ShouldShowAppMenuPrompt() {
  PrefService *local_state = g_browser_process->local_state();
  const PrefService::Preference *first_shown_time_pref =
      local_state->FindPreference(prefs::kDefaultBrowserFirstShownTime);
  base::Time first_shown_time =
      local_state->GetTime(prefs::kDefaultBrowserFirstShownTime);

  return first_shown_time_pref->IsDefaultValue() ||
         (base::Time::Now() - first_shown_time) <
             features::kDefaultBrowserAppMenuDuration.Get();
}
} // namespace

void chrome::startup::default_prompt::ResetPromptPrefs(Profile *profile) {
  profile->GetPrefs()->ClearPref(prefs::kDefaultBrowserLastDeclined);

  PrefService *local_state = g_browser_process->local_state();
  local_state->ClearPref(prefs::kDefaultBrowserLastDeclinedTime);
  local_state->ClearPref(prefs::kDefaultBrowserDeclinedCount);
  local_state->ClearPref(prefs::kDefaultBrowserFirstShownTime);
}

void chrome::startup::default_prompt::UpdatePrefsForDismissedPrompt(
    Profile *profile) {
  base::Time now = base::Time::Now();
  profile->GetPrefs()->SetInt64(prefs::kDefaultBrowserLastDeclined,
                                now.ToInternalValue());

  PrefService *local_state = g_browser_process->local_state();
  local_state->SetTime(prefs::kDefaultBrowserLastDeclinedTime, now);
  local_state->SetInteger(
      prefs::kDefaultBrowserDeclinedCount,
      local_state->GetInteger(prefs::kDefaultBrowserDeclinedCount) + 1);
  local_state->ClearPref(prefs::kDefaultBrowserFirstShownTime);
}

void chrome::startup::default_prompt::MaybeResetAppMenuPromptPrefs(
    Profile *profile) {
  if (!base::FeatureList::IsEnabled(features::kDefaultBrowserPromptRefresh) ||
      !features::kShowDefaultBrowserAppMenuChip.Get()) {
    g_browser_process->local_state()->ClearPref(
        prefs::kDefaultBrowserFirstShownTime);
    return;
  }

  if (!ShouldShowAppMenuPrompt()) {
    // Found that app menu should no longer be shown. Triggers an implicit
    // dismissal so that the subsequent call to ShouldShowPrompts() will return
    // false.
    UpdatePrefsForDismissedPrompt(profile);
  }
}
