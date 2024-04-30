// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_MANAGER_PREF_NAMES_H_
#define COMPONENTS_USER_MANAGER_USER_MANAGER_PREF_NAMES_H_

namespace user_manager::prefs {

// Preferences for user_manager in the LocalStore.
// Please keep the lexicographical order if new entry is added.
//
// Note: for compatibility and historical reasons, multi-user-sign-in related
// preferences are using the name "multi Profile" here.

// Key name of a dictionary in local state to store cached multi user
// sign-in behavior policy value.
inline constexpr char kCachedMultiProfileUserBehavior[] =
    "CachedMultiProfileUserBehavior";

// Gets set when a device local account is removed but a user is currently
// logged into that account, requiring the account's data to be removed
// after logout.
inline constexpr char kDeviceLocalAccountPendingDataRemoval[] =
    "PublicAccountPendingDataRemoval";

// A list pref of the device local accounts defined on this device. Note that
// this is separate from kAccountsPrefDeviceLocalAccounts because it reflects
// the accounts that existed on the last run of Chrome and therefore have saved
// data.
inline constexpr char kDeviceLocalAccountsWithSavedData[] = "PublicAccounts";

// A string pref containing the ID of the last active user.
// In case of browser crash, this pref will be used to set active user after
// session restore.
inline constexpr char kLastActiveUser[] = "LastActiveUser";

// A string pref containing the ID of the last user who logged in if it was
// a user with gaia account (regular) or an empty string if it was another type
// of user (guest, kiosk, public account, etc.).
inline constexpr char kLastLoggedInGaiaUser[] = "LastLoggedInRegularUser";

// Stores a dictionary that describes who is the owner user of the device.
// If present, currently always contains "type": 1 (i.e. kGoogleEmail) and
// "account" that holds of the email of the owner user.
inline constexpr char kOwnerAccount[] = "owner.account";

// Inner fields for the kOwnerAccount dict.
inline constexpr char kOwnerAccountIdentity[] = "account";
inline constexpr char kOwnerAccountType[] = "type";

// A list pref of the the regular users known on this device, arranged in LRU
// order, stored in local state.
inline constexpr char kRegularUsersPref[] = "LoggedInUsers";

// A dictionary that maps user IDs to the displayed (non-canonical) emails.
inline constexpr char kUserDisplayEmail[] = "UserDisplayEmail";

// A dictionary that maps user IDs to the displayed name.
inline constexpr char kUserDisplayName[] = "UserDisplayName";

// A dictionary that maps user IDs to a flag indicating whether online
// authentication against GAIA should be enforced during the next sign-in.
inline constexpr char kUserForceOnlineSignin[] = "UserForceOnlineSignin";

// A dictionary that maps user IDs to the user's given name.
inline constexpr char kUserGivenName[] = "UserGivenName";

// A dictionary that maps user IDs to OAuth token presence flag.
inline constexpr char kUserOAuthTokenStatus[] = "OAuthTokenStatus";

// A dictionary that maps user ID to the user type.
inline constexpr char kUserType[] = "UserType";

// Preferences for user_manager in each Profile preferences.
// Please keep the lexicographical order if new entry is added.

// A boolean pref recording whether user has dismissed the multiprofile
// introduction dialog show.
inline constexpr char kMultiProfileNeverShowIntro[] =
    "settings.multi_profile_never_show_intro";

// A string pref that holds string enum values of how the user should behave
// in a multi-user sign-in session. See ChromeOsMultiProfileUserBehavior policy
// for more details of the valid values.
inline constexpr char kMultiProfileUserBehaviorPref[] =
    "settings.multiprofile_user_behavior";

// A boolean pref recording whether user has dismissed the multiprofile
// teleport warning dialog show.
inline constexpr char kMultiProfileWarningShowDismissed[] =
    "settings.multi_profile_warning_show_dismissed";

}  // namespace user_manager::prefs

#endif  // COMPONENTS_USER_MANAGER_USER_MANAGER_PREF_NAMES_H_
