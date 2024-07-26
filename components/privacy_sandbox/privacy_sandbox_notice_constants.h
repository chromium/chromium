// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Temporary file that stores existing Privacy Sandbox Notice related constants
// across different notice types and different surfaces.
#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_CONSTANTS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_CONSTANTS_H_

#include <string_view>

#include "base/containers/fixed_flat_set.h"
namespace privacy_sandbox {

inline constexpr int kPrivacySandboxNoticeSchemaVersion = 1;

// Topics Consent modal names.
inline constexpr char kTopicsConsentModal[] = "TopicsConsentDesktopModal";
inline constexpr char kTopicsConsentModalClankBrApp[] =
    "TopicsConsentModalClankBrApp";
inline constexpr char kTopicsConsentModalClankCCT[] =
    "TopicsConsentModalClankCCT";

// Reminder notice names.
inline constexpr char kTrackingProtectionSilentReminderClank[] =
    "TrackingProtectionSilentReminderClankBrApp";
inline constexpr char kTrackingProtectionReminderClank[] =
    "TrackingProtectionReminderClankBrApp";
inline constexpr char kTrackingProtectionSilentReminderDesktopIPH[] =
    "TrackingProtectionSilentReminderDesktopIPH";
inline constexpr char kTrackingProtectionReminderDesktopIPH[] =
    "TrackingProtectionReminderDesktopIPH";

inline constexpr auto kPrivacySandboxNoticeNames =
    base::MakeFixedFlatSet<std::string_view>(
        {kTopicsConsentModal, kTopicsConsentModalClankBrApp,
         kTopicsConsentModalClankCCT, kTrackingProtectionSilentReminderClank,
         kTrackingProtectionReminderClank,
         kTrackingProtectionSilentReminderDesktopIPH,
         kTrackingProtectionReminderDesktopIPH});

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_CONSTANTS_H_
