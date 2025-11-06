// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_LIMIT_MANAGER_H_
#define CHROME_BROWSER_UI_TABS_TAB_LIMIT_MANAGER_H_

class Profile;

namespace tab_limit_manager {

// Maximum number of regular (non-incognito) tabs allowed.
constexpr int kMaxRegularTabs = 4;

// Maximum number of incognito tabs allowed.
constexpr int kMaxIncognitoTabs = 1;

// Returns the number of regular (non-incognito) tabs across all browser
// windows for the given profile and related profiles.
int CountRegularTabs(Profile* profile);

// Returns the number of incognito tabs across all browser windows for the
// given profile's off-the-record profile.
int CountIncognitoTabs(Profile* profile);

// Checks if adding a new tab would exceed the tab limit.
// Returns true if the tab can be added, false if the limit would be exceeded.
bool CanAddNewTab(Profile* profile, bool is_incognito);

}  // namespace tab_limit_manager

#endif  // CHROME_BROWSER_UI_TABS_TAB_LIMIT_MANAGER_H_
