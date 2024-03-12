// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_H_

class PrefRegistrySimple;
class PrefService;
class Profile;

void RegisterDefaultBrowserPromptPrefs(PrefRegistrySimple* registry);

// Migrates the old last declined time profile pref to the new last declined
// time local pref.
void MigrateDefaultBrowserLastDeclinedPref(PrefService* profile_prefs);

// Shows a prompt UI to set the default browser if necessary.
void ShowDefaultBrowserPrompt(Profile* profile);

// Marks the default browser prompt as having been declined.
void DefaultBrowserPromptDeclined(Profile* profile);

// Resets the tracking preference for the default browser prompt so that it is
// re-shown if the browser ceases to be the user's chosen default.
void ResetDefaultBrowserPrompt(Profile* profile);

// Only used within tests to confirm the behavior of the default browser prompt.
void ShowPromptForTesting();

// Only used within tests to confirm the triggering logic for the default
// browser prompt.
bool ShouldShowDefaultBrowserPromptForTesting(Profile* profile);

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_H_
