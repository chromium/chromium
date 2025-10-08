// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

void chrome::startup::default_prompt::ResetPromptPrefs(Profile* profile) {
  profile->GetPrefs()->ClearPref(prefs::kDefaultBrowserLastDeclined);

  PrefService* local_state = g_browser_process->local_state();
  local_state->ClearPref(prefs::kDefaultBrowserLastDeclinedTime);
  local_state->ClearPref(prefs::kDefaultBrowserDeclinedCount);
}

void chrome::startup::default_prompt::UpdatePrefsForDismissedPrompt(
    Profile* profile) {
  base::Time now = base::Time::Now();
  profile->GetPrefs()->SetInt64(prefs::kDefaultBrowserLastDeclined,
                                now.ToInternalValue());

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetTime(prefs::kDefaultBrowserLastDeclinedTime, now);
  local_state->SetInteger(
      prefs::kDefaultBrowserDeclinedCount,
      local_state->GetInteger(prefs::kDefaultBrowserDeclinedCount) + 1);
}
