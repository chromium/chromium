// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_TRIAL_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_TRIAL_H_

#include <string>

class DefaultBrowserPromptTrial {
 public:
  DefaultBrowserPromptTrial() = delete;

  DefaultBrowserPromptTrial(const DefaultBrowserPromptTrial&) = delete;
  DefaultBrowserPromptTrial& operator=(const DefaultBrowserPromptTrial&) =
      delete;

  // Enrolls this client with a synthetic field trial based on the Finch params.
  // Should be called when the default browser prompt is potentially shown, then
  // the client needs to register again on each process startup by calling
  // `EnsureStickToDefaultBrowserPromptCohort()`.
  static void MaybeJoinDefaultBrowserPromptCohort();

  // Ensures that the user's experiment group is appropriately reported to track
  // the effect of the default browser prompt over time. Should be called once
  // per browser process startup.
  static void EnsureStickToDefaultBrowserPromptCohort();

 private:
  // Reports to the launch study for the default browser prompt synthetic trial.
  static void RegisterSyntheticFieldTrial(const std::string& group_name);
};

#endif // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_DEFAULT_BROWSER_PROMPT_TRIAL_H_
