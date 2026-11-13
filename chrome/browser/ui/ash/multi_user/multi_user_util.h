// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_UTIL_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_UTIL_H_

class AccountId;
class Profile;

namespace multi_user_util {

// Get the user id from a given profile.
AccountId GetAccountIdFromProfile(const Profile* profile);

// Get a profile for a given user id.
Profile* GetProfileFromAccountId(const AccountId& account_id);

// Check if the given profile is from the currently active user. Note that since
// only Chrome OS has the concept of an active user, only Chrome OS can ever
// return false.
bool IsProfileFromActiveUser(Profile* profile);

}  // namespace multi_user_util

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_UTIL_H_
