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

}  // namespace prefs

namespace privacy_sandbox {

// Registers user preferences related to privacy sandbox.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// If the Ad Privacy Deprecation feature is enabled, this function will set all
// the Ad API prefs to it's default value.
void MaybeClearAdPrivacyPrefs(PrefService* pref_service);

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_
