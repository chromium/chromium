// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_H_

class PrefRegistrySimple;
class Profile;

void RegisterDefaultBrowserPromptPrefs(PrefRegistrySimple* registry);

// Shows a prompt UI to set the default browser if necessary.
void ShowDefaultBrowserPrompt(Profile* profile);

// Marks the default browser prompt as having been declined.
void DefaultBrowserPromptDeclined(Profile* profile);

// Resets the tracking preference for the default browser prompt so that it is
// re-shown if the browser ceases to be the user's chosen default.
void ResetDefaultBrowserPrompt(Profile* profile);

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_H_
