// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_PREFERENCES_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_PREFERENCES_H_

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "url/gurl.h"

// Functions in this module should take a preferences service as an argument and
// perform operations on it that manipulate the preferences related to the
// family.
namespace supervised_user {

// Register preferences that describe parental controls.
void RegisterFamilyPrefs(
    PrefService& pref_service,
    const kids_chrome_management::ListMembersResponse& response);
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Set preferences that describe parental controls.
void EnableParentalControls(PrefService& pref_service);
void DisableParentalControls(PrefService& pref_service);

bool IsChildAccountStatusKnown(const PrefService& pref_service);

// Returns true if the safe sites preference is enabled and user is supervised.
bool IsSafeSitesEnabled(const PrefService& pref_service);

// Returns true if both the primary account is a child account subject to
// parental controls and the platform supports Family Link supervision features.
bool IsSubjectToParentalControls(const PrefService& pref_service);

// Returns true if the extensions permissions parental control is enabled
// for supervised users.
// Returns false if the user is not supervised.
bool AreExtensionsPermissionsEnabled(const PrefService& pref_service);

// Returns true if the extension handling mode for skipping parent approval is
// enabled and the parent has authorized installing extensions without their
// approval.
// Returns false if the user is not supervised.
bool SupervisedUserCanSkipExtensionParentApprovals(
    const PrefService& pref_service);
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_PREFERENCES_H_
