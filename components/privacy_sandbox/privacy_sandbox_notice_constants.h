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

// Full 3PCD notice names.
inline constexpr char kFull3PCDIPH[] = "Full3PCDDesktopIPH";
inline constexpr char kFull3PCDClankBrApp[] = "Full3PCDClankBrApp";
inline constexpr char kFull3PCDClankCCT[] = "Full3PCDClankCCT";

// Full 3PCD silent notice names.
inline constexpr char kFull3PCDSilentIPH[] = "Full3PCDSilentDesktopIPH";
inline constexpr char kFull3PCDSilentClankBrApp[] = "Full3PCDSilentClankBrApp";
inline constexpr char kFull3PCDSilentClankCCT[] = "Full3PCDSilentClankCCT";

// Full 3PCD with IPP notice names.
inline constexpr char kFull3PCDWithIPPIPH[] = "Full3PCDWithIPPDesktopIPH";
inline constexpr char kFull3PCDWithIPPClankBrApp[] =
    "Full3PCDWithIPPClankBrApp";
inline constexpr char kFull3PCDWithIPPClankCCT[] = "Full3PCDWithIPPClankCCT";

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
        {kFull3PCDIPH, kFull3PCDClankBrApp, kFull3PCDClankCCT,
         kTopicsConsentModal, kTopicsConsentModalClankBrApp,
         kTopicsConsentModalClankCCT, kTrackingProtectionSilentReminderClank,
         kTrackingProtectionReminderClank,
         kTrackingProtectionSilentReminderDesktopIPH,
         kTrackingProtectionReminderDesktopIPH, kFull3PCDWithIPPIPH,
         kFull3PCDWithIPPClankBrApp, kFull3PCDWithIPPClankCCT,
         kFull3PCDSilentIPH, kFull3PCDSilentClankBrApp,
         kFull3PCDSilentClankCCT});

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_CONSTANTS_H_
