// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_PREFERENCES_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_PREFERENCES_H_

#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"

// Functions in this module should take a preferences service as an argument and
// perform operations on it that manipulate the preferences related to the
// family.
namespace supervised_user {

void RegisterFamilyPrefs(
    PrefService& pref_service,
    const kids_chrome_management::ListFamilyMembersResponse& response);

// Sets preferences that describe parental controls.
void EnableParentalControls(PrefService& pref_service);
void DisableParentalControls(PrefService& pref_service);

bool IsChildAccountStatusKnown(PrefService& pref_service);

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_PREFERENCES_H_
