// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_H_

#include "base/functional/callback_forward.h"

class PrefRegistrySimple;
class PrefService;
class Profile;

void RegisterDefaultBrowserPromptPrefs(PrefRegistrySimple* registry);

// Migrates the old last declined time profile pref to the new last declined
// time local pref.
void MigrateDefaultBrowserLastDeclinedPref(PrefService* profile_prefs);

// Shows a prompt UI to set the default browser if necessary. Passes a bool
// indicating whether or not the prompt was shown to `done_callback` when done.
void ShowDefaultBrowserPrompt(Profile* profile,
                              base::OnceCallback<void(bool)> done_callback);

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_H_
