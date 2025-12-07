// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_HISTOGRAMS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRIVACY_SANDBOX_ATTESTATIONS_HISTOGRAMS_H_

#include "build/buildflag.h"

namespace privacy_sandbox {

#if BUILDFLAG(IS_ANDROID)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// This enum is used for recording the status of loading attestations component
// from Android APK assets. The differences between this histogram and the
// parsing status histogram `kAttestationsFileParsingStatusUMA` are:
// 1. The parsing status only concerns the parsing of the attestations list. The
// manifest parsing is done by the component updater. However, for loading from
// APK assets, both manifest and attestations list parsing are done by the
// attestations class.
// 2. This histogram has more granular error conditions since there are more
// steps required to open files from APK assets.
enum class LoadAPKAssetStatus {
  kSuccess = 0,
  kCannotOpenList = 1,
  kCannotMemoryMapList = 2,
  kCannotParseList = 3,
  kMaxValue = kCannotParseList,
};

inline constexpr char kAttestationsLoadAPKAssetStatusUMA[] =
    "PrivacySandbox.Attestations.LoadAPKAssetStatus";
#endif  // BUILDFLAG(IS_ANDROID)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Note: The attestation component used to have a sentinel guard. It creates a
// sentinel file when attestation parsing crashes to prevent subsequent parsing
// attempts. It has been removed. The enum entries related to this sentinel
// guard is no longer used.
//
// TODO(crbug.com/351843800): Clean up unused enums and update histogram
// "PrivacySandbox.Attestations.Parsing.Status".
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

enum class FileSource {
  kPreInstalled = 0,
  kDownloaded = 1,
  kMaxValue = kDownloaded,
};

inline constexpr char kAttestationStatusUMA[] =
    "PrivacySandbox.Attestations.IsSiteAttestedStatus";
inline constexpr char kAttestationFirstCheckTimeUMA[] =
    "PrivacySandbox.Attestations.IsSiteAttested.FirstCheckTime";
inline constexpr char kAttestationsFileParsingStatusUMA[] =
    "PrivacySandbox.Attestations.Parsing.Status";
inline constexpr char kAttestationsFileSource[] =
    "PrivacySandbox.Attestations.IsSiteAttested.FileSource";
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
