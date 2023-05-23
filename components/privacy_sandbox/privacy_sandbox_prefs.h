// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_

class PrefRegistrySimple;

namespace prefs {

// Un-synced boolean pref indicating whether the consent decision was made.
extern const char kPrivacySandboxM1ConsentDecisionMade[];
// Un-synced boolean pref indicating whether the notice was acknowledged.
extern const char kPrivacySandboxM1EEANoticeAcknowledged[];
// Un-synced boolean pref indicating whether the notice shown in ROW was
// acknowledged.
extern const char kPrivacySandboxM1RowNoticeAcknowledged[];
// Un-synced boolean pref indicating whether the restricted notice was
// acknowledged.
extern const char kPrivacySandboxM1RestrictedNoticeAcknowledged[];
// Un-synced integer pref indicating PromptSuppressedReason for the prompt.
extern const char kPrivacySandboxM1PromptSuppressed[];
// Un-synced boolean pref indicating if Topics API is enabled.
extern const char kPrivacySandboxM1TopicsEnabled[];
// Un-synced boolean pref indicating if Fledge API is enabled.
extern const char kPrivacySandboxM1FledgeEnabled[];
// Un-synced boolean pref indicating if Ad measurement API is enabled.
extern const char kPrivacySandboxM1AdMeasurementEnabled[];
// Un-synced boolean pref indicating if the Privacy Sandbox was ever indicated
// as restricted by account capabilities.
extern const char kPrivacySandboxM1Restricted[];
// Un-synced boolean pref indicating if the Privacy Sandbox was ever indicated
// as unrestricted by account capabilities.
// TODO(crbug.com/1428506): Deprecate this preference
extern const char kPrivacySandboxM1Unrestricted[];

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
// TODO(crbug.com/1292898): Deprecate this preference once all users have been
// migrated to the V2 pref.
extern const char kPrivacySandboxManuallyControlled[];

// Un-synced boolean pref. This is a replacement for the synced preference
// above. It it set to true when the user manually toggles the setting on the
// updated settings page.
extern const char kPrivacySandboxManuallyControlledV2[];

// Boolean that indicates whether the privacy sandbox desktop page at
// chrome://settings/privacySandbox has been viewed.
extern const char kPrivacySandboxPageViewed[];

// The point in time from which history is eligible to be used when calculating
// a user's Topics API topics.
extern const char kPrivacySandboxTopicsDataAccessibleSince[];

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

// Boolean that indicates a Privacy Sandbox confirmation was not shown to the
// profile because the Privacy Sandbox is being manually controlled.
extern const char kPrivacySandboxNoConfirmationManuallyControlled[];

// Boolean that indicates the user's Privacy Sandbox setting was disabled
// automatically because they do not have the correct level of confirmation.
extern const char kPrivacySandboxDisabledInsufficientConfirmation[];

// Boolean that indicates the user's FPS data access preference has been init,
// so named because of the user intent it intends to represent. Currently there
// is no distinction between FPS for data access, and FPS for other purposes, so
// this init is applied to the first_party_sets.enabled pref.
extern const char kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized[];

// Boolean that indicates whether First-Party Sets is enabled. Exposed to the
// user via Chrome UI, and to enterprises via enterprise policy.
extern const char kPrivacySandboxFirstPartySetsEnabled[];

// Boolean that stores the users Topics consent status, true when the user has
// an active Topics consent, false otherwise. This is specifically separate
// from the kPrivacySandboxM1TopicsEnabled preference, which may be overridden
// by policy or extensions.
extern const char kPrivacySandboxTopicsConsentGiven[];

// Timestamp that stores the last time the user made a consent decision for
// Topics, in either settings or as part of a confirmation moment.
extern const char kPrivacySandboxTopicsConsentLastUpdateTime[];

// Enum that stores the reason that the Topics consent is in the current state,
// stores one of the values of `TopicsConsentUpdateSource`.
extern const char kPrivacySandboxTopicsConsentLastUpdateReason[];

// String that stores the complete, localized, text of the consent moment which
// resulted in the current Topics consent state.
extern const char kPrivacySandboxTopicsConsentTextAtLastUpdate[];

// Boolean that indicates whether the user's anti-abuse preference has been
// initialized.
extern const char kPrivacySandboxAntiAbuseInitialized[];

}  // namespace prefs

namespace privacy_sandbox {

// Represents the different ways in which the Topics consent state could be
// updated.
enum class TopicsConsentUpdateSource {
  kDefaultValue = 0,
  kConfirmation = 1,
  kSettings = 2,
};

// Registers user preferences related to privacy sandbox.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_
