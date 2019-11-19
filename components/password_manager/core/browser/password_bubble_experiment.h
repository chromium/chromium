// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_

class PrefRegistrySimple;
class PrefService;

namespace syncer {
class SyncService;
}

namespace password_bubble_experiment {

// Registers prefs which controls appearance of the first run experience for the
// Smart Lock UI, namely was first run experience shown for save prompt or auto
// sign-in prompt.
void RegisterPrefs(PrefRegistrySimple* registry);

// Returns the number of times the "Save password" bubble can be dismissed by
// user before it's not shown automatically.
int GetSmartBubbleDismissalThreshold();

// Returns true if the user syncs passwords to Google Account.
// TODO(crbug.com/862269): rename the function.
bool IsSmartLockUser(const syncer::SyncService* sync_service);

// Returns true if first run experience for auto sign-in prompt should be shown.
bool ShouldShowAutoSignInPromptFirstRunExperience(PrefService* prefs);

// Sets appropriate value to the preference which controls appearance of the
// first run experience for the auto sign-in prompt.
void RecordAutoSignInPromptFirstRunExperienceWasShown(PrefService* prefs);

// Turns off the auto signin experience setting.
void TurnOffAutoSignin(PrefService* prefs);

// Returns true if the Chrome Sign In promo should be shown.
bool ShouldShowChromeSignInPasswordPromo(
    PrefService* prefs,
    const syncer::SyncService* sync_service);

}  // namespace password_bubble_experiment

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_BUBBLE_EXPERIMENT_H_
