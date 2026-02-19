// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/scheduler/manifest_silent_update_result.h"

#include <ostream>
#include <string>
#include <utility>

#include "base/i18n/time_formatting.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/values.h"

namespace web_app {

bool IsAppUpdated(ManifestSilentUpdateCheckResult result) {
  switch (result) {
    case ManifestSilentUpdateCheckResult::kAppSilentlyUpdated:
    case ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate:
    case ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges:
    case ManifestSilentUpdateCheckResult::kAppHasSecurityUpdateDueToThrottle:
    case ManifestSilentUpdateCheckResult::
        kAppSilentlyUpdatedDueToSmallIconComparison:
      return true;
    case ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall:
    case ManifestSilentUpdateCheckResult::kSystemShutdown:
    case ManifestSilentUpdateCheckResult::kAppUpToDate:
    case ManifestSilentUpdateCheckResult::kIconReadFromDiskFailed:
    case ManifestSilentUpdateCheckResult::kWebContentsWasDestroyed:
    case ManifestSilentUpdateCheckResult::kPendingIconWriteToDiskFailed:
    case ManifestSilentUpdateCheckResult::kInvalidManifest:
    case ManifestSilentUpdateCheckResult::kInvalidPendingUpdateInfo:
    case ManifestSilentUpdateCheckResult::kUserNavigated:
    case ManifestSilentUpdateCheckResult::kManifestToWebAppInstallInfoError:
    case ManifestSilentUpdateCheckResult::kAppNotAllowedToUpdate:
      return false;
  }
}

ManifestSilentUpdateCompletionInfo::ManifestSilentUpdateCompletionInfo() =
    default;

ManifestSilentUpdateCompletionInfo::ManifestSilentUpdateCompletionInfo(
    ManifestSilentUpdateCheckResult result)
    : result(result) {}

ManifestSilentUpdateCompletionInfo::ManifestSilentUpdateCompletionInfo(
    ManifestSilentUpdateCompletionInfo&&) = default;

ManifestSilentUpdateCompletionInfo&
ManifestSilentUpdateCompletionInfo::operator=(
    ManifestSilentUpdateCompletionInfo&&) = default;

base::DictValue ManifestSilentUpdateCompletionInfo::ToDebugValue() {
  return base::DictValue()
      .Set("result", base::ToString(result))
      .Set("time_for_icon_diff_check",
           time_for_icon_diff_check.has_value()
               ? base::TimeFormatShortDateAndTime(
                     time_for_icon_diff_check.value())
               : base::EmptyString16());
}

std::string ManifestSilentUpdateCompletionInfo::ToString() {
  return ToDebugValue().DebugString();
}

std::ostream& operator<<(std::ostream& os,
                         ManifestSilentUpdateCheckResult result) {
  switch (result) {
    case ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall:
      return os << "kAppUpdateFailedDuringInstall";
    case ManifestSilentUpdateCheckResult::kSystemShutdown:
      return os << "kSystemShutdown";
    case ManifestSilentUpdateCheckResult::kAppSilentlyUpdated:
      return os << "kAppSilentlyUpdated";
    case ManifestSilentUpdateCheckResult::kAppUpToDate:
      return os << "kAppUpToDate";
    case ManifestSilentUpdateCheckResult::kIconReadFromDiskFailed:
      return os << "kIconReadFromDiskFailed";
    case ManifestSilentUpdateCheckResult::kWebContentsWasDestroyed:
      return os << "kWebContentsWasDestroyed";
    case ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate:
      return os << "kAppOnlyHasSecurityUpdate";
    case ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges:
      return os << "kAppHasNonSecurityAndSecurityChanges";
    case ManifestSilentUpdateCheckResult::kPendingIconWriteToDiskFailed:
      return os << "kPendingIconWriteToDiskFailed";
    case ManifestSilentUpdateCheckResult::kInvalidManifest:
      return os << "kInvalidManifest";
    case ManifestSilentUpdateCheckResult::kInvalidPendingUpdateInfo:
      return os << "kInvalidPendingUpdateInfo";
    case ManifestSilentUpdateCheckResult::kUserNavigated:
      return os << "kUserNavigated";
    case ManifestSilentUpdateCheckResult::kManifestToWebAppInstallInfoError:
      return os << "kManifestToWebAppInstallInfoError";
    case ManifestSilentUpdateCheckResult::kAppHasSecurityUpdateDueToThrottle:
      return os << "kAppHasSecurityUpdateDueToThrottle";
    case ManifestSilentUpdateCheckResult::kAppNotAllowedToUpdate:
      return os << "kAppNotAllowedToUpdate";
    case ManifestSilentUpdateCheckResult::
        kAppSilentlyUpdatedDueToSmallIconComparison:
      return os << "kAppSilentlyUpdatedDueToSmallIconComparison";
  }
}

}  // namespace web_app
