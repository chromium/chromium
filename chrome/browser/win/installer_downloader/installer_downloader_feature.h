// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_FEATURE_H_
#define CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_FEATURE_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace installer_downloader {

// When enabled, eligible Windows 10 users may experience up to kMaxCycleCount
// re-engagement campaign cycles separated by kReengagementCooldownDays.
// Within each cycle, existing behavior is repeated (shown up to 3 times if
// ignored, or stops immediately if explicitly dismissed).
BASE_DECLARE_FEATURE(kInstallerDownloaderReengagement);

// Max number of re-engagement campaign cycles.
BASE_DECLARE_FEATURE_PARAM(int, kMaxCycleCount);

// Cooldown period (in days) between re-engagement campaign cycles.
BASE_DECLARE_FEATURE_PARAM(int, kReengagementCooldownDays);

}  // namespace installer_downloader

#endif  // CHROME_BROWSER_WIN_INSTALLER_DOWNLOADER_INSTALLER_DOWNLOADER_FEATURE_H_

