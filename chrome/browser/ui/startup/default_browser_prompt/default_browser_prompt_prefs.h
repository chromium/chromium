// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_PREFS_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_PREFS_H_

#include "chrome/browser/profiles/profile.h"

namespace chrome::startup::default_prompt {

// Resets the tracking preferences for the default browser prompts so that
// they are re-shown if the browser ceases to be the user's chosen default.
void ResetPromptPrefs(Profile* profile);

// Updates the tracking preferences for the default browser prompts to reflect
// that the prompt was just dismissed. This will ensure the proper delay
// before re-prompting.
void UpdatePrefsForDismissedPrompt(Profile* profile);

// If enough time has passed since the first show time, the app menu should
// implicitly be dismissed, in which case prompts will not be shown when
// `MaybeShowPrompt()` is called.
void MaybeResetAppMenuPromptPrefs(Profile* profile);

}  // namespace chrome::startup::default_prompt

#endif // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_PREFS_H_
