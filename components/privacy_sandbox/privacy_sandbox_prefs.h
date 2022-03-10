// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_

class PrefRegistrySimple;

namespace prefs {

// Synced boolean pref. Privacy Sandbox APIs may only be enabled when this is
// enabled, but each API will respect its own enabling logic if this pref is
// true. When this pref is false ALL Privacy Sandbox APIs are disabled.
// TODO(crbug.com/1292898): Deprecate this preference once all users have been
// migrated to the V2 pref.
extern const char kPrivacySandboxApisEnabled[];

// Un-synced boolean pref. This is a replacement for the synced preference
// above. It performs the exact same functionality, but is unsynced. This
// preference is only consulted when the kPrivacySandboxSettings3 feature is
// enabled.
extern const char kPrivacySandboxApisEnabledV2[];

// Synced boolean that indicates if a user has manually toggled the settings
// associated with the PrivacySandboxSettings feature.
extern const char kPrivacySandboxManuallyControlled[];

// Boolean to indicate whether or not the preferecnes have been reconciled for
// this device. This occurs for each device once when privacy sandbox is first
// enabled.
extern const char kPrivacySandboxPreferencesReconciled[];

// Boolean that indicates whether the privacy sandbox desktop page at
// chrome://settings/privacySandbox has been viewed.
extern const char kPrivacySandboxPageViewed[];

// The point in time from which history is eligible to be used when calculating
// a user's FLoC ID.
// TODO(crbug.com/1292898): Deprecate this preference once Privacy Sandbox
// Settings 3 has been launched.
extern const char kPrivacySandboxFlocDataAccessibleSince[];

// The point in time from which history is eligible to be used when calculating
// a user's Topics API topics.
extern const char kPrivacySandboxTopicsDataAccessibleSince[];

// Synced boolean that controls whether FLoC is enabled. Requires that the
// kPrivacySandboxApisEnabled preference be enabled to take effect.
extern const char kPrivacySandboxFlocEnabled[];

// List of entries representing Topics API topics which are blocked for
// the profile. Blocked topics cannot be provided to site, or considered as
// part of the profile's "top topics". Entries in the list are dictionaries
// containing the CanonicalTopic and the time the setting was created.
extern const char kPrivacySandboxBlockedTopics[];

// Dictionary of entries representing top frame origins on which the profile
// cannot be joined to an interest group. Keys are the blocked origins, and
// values are the time the setting was applied.
extern const char kPrivacySandboxFledgeJoinBlocked[];

// Boolean that indicates that the Privacy Sandbox notice was shown to the
// profile.
extern const char kPrivacySandboxNoticeDisplayed[];

// Boolean that indicates that this profile has made a decision on the Privacy
// Sandbox consent.
extern const char kPrivacySandboxConsentDecisionMade[];

// Boolean that indicates a Privacy Sandbox confirmation was not shown to the
// profile because the profile had already disabled the Privacy Sandbox.
extern const char kPrivacySandboxNoConfirmationSandboxDisabled[];

// Boolean that indicates a Privacy Sandbox confirmation was not shown to the
// profile because the Privacy Sandbox was being restricted.
extern const char kPrivacySandboxNoConfirmationSandboxRestricted[];

// Boolean that indicates a Privacy Sandbox confirmation was not shown to the
// profile because the Privacy Sandbox was being managed.
extern const char kPrivacySandboxNoConfirmationSandboxManaged[];

// Boolean that indicates a Privacy Sandbox confirmation was not shown to the
// profile because the third party cookies were being blocked.
extern const char kPrivacySandboxNoConfirmationThirdPartyCookiesBlocked[];

// Boolean that indicates the user's Privacy Sandbox setting was disabled
// automatically because they do not have the correct level of confirmation.
extern const char kPrivacySandboxDisabledInsufficientConfirmation[];

}  // namespace prefs

namespace privacy_sandbox {

// Registers user preferences related to privacy sandbox.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_
