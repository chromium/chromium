// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_HISTOGRAMS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_HISTOGRAMS_H_

namespace privacy_sandbox {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ParsingStatus {
  kSuccess = 0,
  kNotNewerVersion = 1,
  kFileNotExist = 2,
  kSentinelFilePresent = 3,
  kCannotCreateSentinel = 4,
  kCannotRemoveSentinel = 5,
  kCannotParseFile = 6,
  kMaxValue = kCannotParseFile,
};

inline constexpr char kAttestationStatusUMA[] =
    "PrivacySandbox.Attestations.IsSiteAttested";
inline constexpr char kAttestationsFileParsingStatusUMA[] =
    "PrivacySandbox.Attestations.Parsing.Status";
inline constexpr char kAttestationsFileParsingTimeUMA[] =
    "PrivacySandbox.Attestations.InitializationDuration.Parsing";
inline constexpr char kAttestationsMapMemoryUsageUMA[] =
    "PrivacySandbox.Attestations.EstimateMemoryUsage.AttestationsMap";
inline constexpr char kComponentReadyFromApplicationStartUMA[] =
    "PrivacySandbox.Attestations.InitializationDuration."
    "ComponentReadyFromApplicationStart";
inline constexpr char kComponentReadyFromApplicationStartWithInterruptionUMA[] =
    "PrivacySandbox.Attestations.InitializationDuration."
    "ComponentReadyFromApplicationStartWithInterruption";
inline constexpr char kComponentReadyFromBrowserWindowFirstPaintUMA[] =
    "PrivacySandbox.Attestations.InitializationDuration."
    "ComponentReadyFromBrowserWindowFirstPaint";

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_HISTOGRAMS_H_
