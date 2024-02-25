// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_

class PrefService;

namespace password_bubble_experiment {

// Returns the number of times the "Save password" bubble can be dismissed by
// user before it's not shown automatically.
int GetSmartBubbleDismissalThreshold();

// Returns true if first run experience for auto sign-in prompt should be shown.
bool ShouldShowAutoSignInPromptFirstRunExperience(PrefService* prefs);

// Sets appropriate value to the preference which controls appearance of the
// first run experience for the auto sign-in prompt.
void RecordAutoSignInPromptFirstRunExperienceWasShown(PrefService* prefs);

}  // namespace password_bubble_experiment

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_
