// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_

class PrefRegistrySimple;

namespace prefs {

// Synced boolean that is true when Privacy Sandbox APIs are enabled. If the
// PrivacySandboxSettings feature is enabled, this Boolean is controlled by the
// associated UI; if it is disabled, it is controlled by third party cookie
// blocking settings.
extern const char kPrivacySandboxApisEnabled[];

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
extern const char kPrivacySandboxFlocDataAccessibleSince[];

}  // namespace prefs

namespace privacy_sandbox {

// Registers user preferences related to privacy sandbox.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PREFS_H_
