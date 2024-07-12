// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Temporary file that stores existing Privacy Sandbox Notice related constants
// across different notice types and different surfaces.
#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_CONSTANTS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_CONSTANTS_H_

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

// Full 3PCD with IPP silent notice names.
inline constexpr char kFull3PCDSilentWithIPPIPH[] =
    "Full3PCDSilentWithIPPDesktopIPH";
inline constexpr char kFull3PCDSilentWithIPPClankBrApp[] =
    "Full3PCDSilentWithIPPClankBrApp";
inline constexpr char kFull3PCDSilentWithIPPClankCCT[] =
    "Full3PCDSilentWithIPPClankCCT";

// Topics Consent modal names.
inline constexpr char kTopicsConsentModal[] = "TopicsConsentDesktopModal";
inline constexpr char kTopicsConsentModalClankBrApp[] =
    "TopicsConsentModalClankBrApp";
inline constexpr char kTopicsConsentModalClankCCT[] =
    "TopicsConsentModalClankCCT";

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_CONSTANTS_H_
