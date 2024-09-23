// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_

class PrefRegistrySimple;

namespace prefs {

// Un-synced boolean pref indicating whether the consent decision was made.
inline constexpr char kPrivacySandboxM1ConsentDecisionMade[] =
    "privacy_sandbox.m1.consent_decision_made";

// Un-synced boolean pref indicating whether the notice was acknowledged.
inline constexpr char kPrivacySandboxM1EEANoticeAcknowledged[] =
    "privacy_sandbox.m1.eea_notice_acknowledged";

// Un-synced boolean pref indicating whether the notice shown in ROW was
// acknowledged.
inline constexpr char kPrivacySandboxM1RowNoticeAcknowledged[] =
    "privacy_sandbox.m1.row_notice_acknowledged";

// Un-synced boolean pref indicating whether the restricted notice was
// acknowledged.
inline constexpr char kPrivacySandboxM1RestrictedNoticeAcknowledged[] =
    "privacy_sandbox.m1.restricted_notice_acknowledged";

// Un-synced integer pref indicating PromptSuppressedReason for the prompt.
inline constexpr char kPrivacySandboxM1PromptSuppressed[] =
    "privacy_sandbox.m1.prompt_suppressed";

// Un-synced boolean pref indicating if Topics API is enabled.
inline constexpr char kPrivacySandboxM1TopicsEnabled[] =
    "privacy_sandbox.m1.topics_enabled";

// Un-synced boolean pref indicating if Fledge API is enabled.
inline constexpr char kPrivacySandboxM1FledgeEnabled[] =
    "privacy_sandbox.m1.fledge_enabled";

// Un-synced boolean pref indicating if Ad measurement API is enabled.
inline constexpr char kPrivacySandboxM1AdMeasurementEnabled[] =
    "privacy_sandbox.m1.ad_measurement_enabled";

// Un-synced boolean pref indicating if the Privacy Sandbox was ever indicated
// as restricted by account capabilities.
inline constexpr char kPrivacySandboxM1Restricted[] =
    "privacy_sandbox.m1.restricted";

// The point in time from which history is eligible to be used when calculating
// a user's Topics API topics.
inline constexpr char kPrivacySandboxTopicsDataAccessibleSince[] =
    "privacy_sandbox.topics_data_accessible_since";

// List of entries representing Topics API topics which are blocked for
// the profile. Blocked topics cannot be provided to site, or considered as
// part of the profile's "top topics". Entries in the list are dictionaries
// containing the CanonicalTopic and the time the setting was created.
inline constexpr char kPrivacySandboxBlockedTopics[] =
    "privacy_sandbox.blocked_topics";

// Dictionary of entries representing top frame origins on which the profile
// cannot be joined to an interest group. Keys are the blocked origins, and
// values are the time the setting was applied.
inline constexpr char kPrivacySandboxFledgeJoinBlocked[] =
    "privacy_sandbox.fledge_join_blocked";

// Boolean that indicates that the Privacy Sandbox notice was shown to the
// profile.
inline constexpr char kPrivacySandboxNoticeDisplayed[] =
    "privacy_sandbox.notice_displayed";

// Boolean that indicates that this profile has made a decision on the Privacy
// Sandbox consent.
inline constexpr char kPrivacySandboxConsentDecisionMade[] =
    "privacy_sandbox.consent_decision_made";

// Boolean that indicates a Privacy Sandbox confirmation was not shown to the
// profile because the profile had already disabled the Privacy Sandbox.
inline constexpr char kPrivacySandboxNoConfirmationSandboxDisabled[] =
    "privacy_sandbox.no_confirmation_sandbox_disabled";

// Boolean that indicates a Privacy Sandbox confirmation was not shown to the
// profile because the Privacy Sandbox was being restricted.
inline constexpr char kPrivacySandboxNoConfirmationSandboxRestricted[] =
    "privacy_sandbox.no_confirmation_sandbox_restricted";

// Boolean that indicates a Privacy Sandbox confirmation was not shown to the
// profile because the Privacy Sandbox was being managed.
inline constexpr char kPrivacySandboxNoConfirmationSandboxManaged[] =
    "privacy_sandbox.no_confirmation_sandbox_managed";

// Boolean that indicates a Privacy Sandbox confirmation was not shown to the
// profile because the third party cookies were being blocked.
inline constexpr char kPrivacySandboxNoConfirmationThirdPartyCookiesBlocked[] =
    "privacy_sandbox.no_confirmation_3PC_blocked";

// Boolean that indicates a Privacy Sandbox confirmation was not shown to the
// profile because the Privacy Sandbox is being manually controlled.
inline constexpr char kPrivacySandboxNoConfirmationManuallyControlled[] =
    "privacy_sandbox.no_confirmation_manually_controlled";

// Boolean that indicates the user's Privacy Sandbox setting was disabled
// automatically because they do not have the correct level of confirmation.
inline constexpr char kPrivacySandboxDisabledInsufficientConfirmation[] =
    "privacy_sandbox.disabled_insufficient_confirmation";

// Boolean that indicates the user's FPS data access preference has been init,
// so named because of the user intent it intends to represent. Currently there
// is no distinction between FPS for data access, and FPS for other purposes, so
// this init is applied to the "privacy_sandbox.first_party_sets_enabled" pref.
inline constexpr char
    kPrivacySandboxFirstPartySetsDataAccessAllowedInitialized[] =
        "privacy_sandbox.first_party_sets_data_access_allowed_initialized";

// Boolean that indicates whether Related Website Sets is enabled. Exposed to
// the user via Chrome UI, and to enterprises via enterprise policy.
// "first_party_sets" in the string name is kept for historic reasons to avoid
// migration of a synced Pref.
inline constexpr char kPrivacySandboxRelatedWebsiteSetsEnabled[] =
    "privacy_sandbox.first_party_sets_enabled";

// Boolean that stores the users Topics consent status, true when the user has
// an active Topics consent, false otherwise. This is specifically separate
// from the kPrivacySandboxM1TopicsEnabled preference, which may be overridden
// by policy or extensions.
inline constexpr char kPrivacySandboxTopicsConsentGiven[] =
    "privacy_sandbox.topics_consent.consent_given";

// Timestamp that stores the last time the user made a consent decision for
// Topics, in either settings or as part of a confirmation moment.
inline constexpr char kPrivacySandboxTopicsConsentLastUpdateTime[] =
    "privacy_sandbox.topics_consent.last_update_time";

// Enum that stores the reason that the Topics consent is in the current state,
// stores one of the values of `TopicsConsentUpdateSource`.
inline constexpr char kPrivacySandboxTopicsConsentLastUpdateReason[] =
    "privacy_sandbox.topics_consent.last_update_reason";

// String that stores the complete, localized, text of the consent moment which
// resulted in the current Topics consent state.
inline constexpr char kPrivacySandboxTopicsConsentTextAtLastUpdate[] =
    "privacy_sandbox.topics_consent.text_at_last_update";

// Pref which contains a list of the activity type from recent chrome launches.
// Version 2 after enum values changed.
inline constexpr char kPrivacySandboxActivityTypeRecord2[] =
    "privacy_sandbox.activity_type.record2";

// Pref that records the timestamp of when a profile was shown a sentiment
// survey.
inline constexpr char kPrivacySandboxSentimentSurveyLastSeen[] =
    "privacy_sandbox.sentiment_survey.last_seen";

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
