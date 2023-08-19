// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILES_PROFILE_VIEW_UTILS_H_
#define CHROME_BROWSER_UI_PROFILES_PROFILE_VIEW_UTILS_H_

#include <string>
#include <utility>
#include <vector>

class Profile;
class ProfileAttributesEntry;
struct AccountInfo;

// Navigates to the Google Account page.
void NavigateToGoogleAccountPage(Profile* profile, const std::string& email);

// Returns true if account sync is paused.
bool IsSyncPaused(Profile* profile);

// Returns true if there is an unconstented profile.
bool HasUnconstentedProfile(Profile* profile);

// Returns the number of browsers associated with |profile|.
// Note: For regular profiles this includes incognito sessions.
int CountBrowsersFor(Profile* profile);

// Returns the AccountInfo from the profile.
AccountInfo GetAccountInfoFromProfile(const Profile* profile);

// Returns the ProfileAttributesEntry from the profile.
ProfileAttributesEntry* GetProfileAttributesFromProfile(const Profile* profile);

// Returns the profile display name based off the profile attributes.
std::u16string GetProfileMenuDisplayName(
    ProfileAttributesEntry* profile_attributes);

// Returns all profile entries sorted by local profile name except for the
// current or omitted profiles.
std::vector<ProfileAttributesEntry*> GetAllOtherProfileEntriesForProfileSubMenu(
    const Profile* current_profile);

// Returns true if |command_id| identifies an other profile menu item.
bool IsOtherProfileCommand(int command_id);

#endif  // CHROME_BROWSER_UI_PROFILES_PROFILE_VIEW_UTILS_H_
