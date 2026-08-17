// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace prefs {

// Ads prefs

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

// RWS prefs

// Boolean that indicates the user's RWS data access preference has been init,
// so named because of the user intent it intends to represent. Currently there
// is no distinction between RWS for data access, and RWS for other purposes, so
// this init is applied to the "privacy_sandbox.first_party_sets_enabled" pref.
// "first_party_sets" in the string name is kept for historic reasons to avoid
// migration of a synced Pref.
inline constexpr char
    kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized[] =
        "privacy_sandbox.first_party_sets_data_access_allowed_initialized";

// Boolean that indicates whether Related Website Sets is enabled. Exposed to
// the user via Chrome UI, and to enterprises via enterprise policy.
// "first_party_sets" in the string name is kept for historic reasons to avoid
// migration of a synced Pref.
inline constexpr char kPrivacySandboxRelatedWebsiteSetsEnabled[] =
    "privacy_sandbox.first_party_sets_enabled";

}  // namespace prefs

namespace privacy_sandbox {

// Registers user preferences related to privacy sandbox.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// If the Ad Privacy Deprecation feature is enabled, this function will set all
// the Ad API prefs to it's default value.
void MaybeClearAdPrivacyPrefs(PrefService* pref_service);

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_
