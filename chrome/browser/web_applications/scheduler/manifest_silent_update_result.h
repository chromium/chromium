// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_MANIFEST_SILENT_UPDATE_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_MANIFEST_SILENT_UPDATE_RESULT_H_

#include <iosfwd>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/values.h"

namespace web_app {

// LINT.IfChange(ManifestSilentUpdateCheckResult)
enum class ManifestSilentUpdateCheckResult {
  kAppUpdateFailedDuringInstall = 1,
  kSystemShutdown = 2,
  kAppSilentlyUpdated = 3,
  kAppUpToDate = 4,
  kIconReadFromDiskFailed = 5,
  kWebContentsWasDestroyed = 6,
  kAppOnlyHasSecurityUpdate = 7,
  kAppHasNonSecurityAndSecurityChanges = 8,
  kPendingIconWriteToDiskFailed = 9,
  kInvalidManifest = 10,
  kInvalidPendingUpdateInfo = 11,
  kUserNavigated = 12,
  kManifestToWebAppInstallInfoError = 13,
  kAppHasSecurityUpdateDueToThrottle = 14,
  kAppNotAllowedToUpdate = 15,
  kAppSilentlyUpdatedDueToSmallIconComparison = 16,
  kMaxValue = kAppSilentlyUpdatedDueToSmallIconComparison,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:WebAppManifestSilentUpdateCheckResult)

bool IsAppUpdated(ManifestSilentUpdateCheckResult result);

struct ManifestSilentUpdateCompletionInfo {
  ManifestSilentUpdateCompletionInfo();
  explicit ManifestSilentUpdateCompletionInfo(
      ManifestSilentUpdateCheckResult result);
  ~ManifestSilentUpdateCompletionInfo() = default;

  base::DictValue ToDebugValue();
  std::string ToString();

  ManifestSilentUpdateCompletionInfo(ManifestSilentUpdateCompletionInfo&&);
  ManifestSilentUpdateCompletionInfo& operator=(
      ManifestSilentUpdateCompletionInfo&&);

  ManifestSilentUpdateCheckResult result;
  std::optional<base::Time> time_for_icon_diff_check;
};

using ManifestSilentUpdateCallback =
    base::OnceCallback<void(ManifestSilentUpdateCompletionInfo check_result)>;

std::ostream& operator<<(std::ostream& os,
                         ManifestSilentUpdateCheckResult result);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_MANIFEST_SILENT_UPDATE_RESULT_H_
