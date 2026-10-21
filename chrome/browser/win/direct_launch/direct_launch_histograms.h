// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_DIRECT_LAUNCH_DIRECT_LAUNCH_HISTOGRAMS_H_
#define CHROME_BROWSER_WIN_DIRECT_LAUNCH_DIRECT_LAUNCH_HISTOGRAMS_H_

namespace direct_launch {

inline constexpr char kDirectLaunchRegistrationResultHistogram[] =
    "Startup.DirectLaunch.RegistrationResult";
inline constexpr char kDirectLaunchTileLaunchHistogram[] =
    "Startup.DirectLaunch.TileLaunch";
inline constexpr char kDirectLaunchSupportedHistogram[] =
    "Startup.DirectLaunch.Supported";
inline constexpr char kDirectLaunchUserDataDirOverrideHistogram[] =
    "Startup.DirectLaunch.UserDataDirOverride";
inline constexpr char kDirectLaunchPayloadResultHistogram[] =
    "Startup.DirectLaunch.PayloadResult";

// Whether a `WM_COPYDATA` payload tagged as a direct launch could be read as a
// URI, and if not, why. Recorded for every tagged payload, so that the share of
// rejections can be read from this histogram alone, and its `kAccepted` bucket
// counts direct URI launches. Rejections tell a shell-side change to the
// payload format apart from the shell no longer sending direct launches at
// all, which the `kAccepted` bucket alone cannot.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DirectLaunchPayloadResult)
enum class DirectLaunchPayloadResult {
  kAccepted = 0,
  kNullBuffer = 1,
  kTooSmall = 2,
  kMisaligned = 3,
  kOversized = 4,
  kEmpty = 5,
  kMaxValue = kEmpty,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/startup/enums.xml:DirectLaunchPayloadResult)

}  // namespace direct_launch

#endif  // CHROME_BROWSER_WIN_DIRECT_LAUNCH_DIRECT_LAUNCH_HISTOGRAMS_H_
