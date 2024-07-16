// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_PREFS_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

class Profile;

namespace tabs {

// Returns the default behavior per platform for tab search position.
bool GetDefaultTabSearchRightAligned();

// Registers Tab Strip specific prefs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Return the value of the preference for TabSearchPosition.
bool GetTabSearchTrailingTabstrip(const Profile* profile);

void SetTabSearchRightAlignedForTesting(bool is_right_aligned);

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_PREFS_H_
