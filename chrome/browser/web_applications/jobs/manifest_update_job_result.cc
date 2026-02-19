// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/manifest_update_job_result.h"

#include <ostream>
#include <string>

#include "base/i18n/time_formatting.h"
#include "base/strings/to_string.h"
#include "base/values.h"

namespace web_app {

std::ostream& operator<<(std::ostream& os, ManifestUpdateJobResult result) {
  switch (result) {
    case ManifestUpdateJobResult::kNoUpdateNeeded:
      os << "kNoUpdateNeeded";
      break;
    case ManifestUpdateJobResult::kSilentlyUpdated:
      os << "kSilentlyUpdated";
      break;
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppOnlyHasSecurityUpdate:
      os << "kPendingUpdateRecorded_AppOnlyHasSecurityUpdate";
      break;
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppHasSecurityUpdateDueToThrottle:
      os << "kPendingUpdateRecorded_AppHasSecurityUpdateDueToThrottle";
      break;
    case ManifestUpdateJobResult::
        kPendingUpdateRecorded_AppHasNonSecurityAndSecurityChanges:
      os << "kPendingUpdateRecorded_AppHasNonSecurityAndSecurityChanges";
      break;
    case ManifestUpdateJobResult::kIconDownloadFailed:
      os << "kIconDownloadFailed";
      break;
    case ManifestUpdateJobResult::kIconReadFromDiskFailed:
      os << "kIconReadFromDiskFailed";
      break;
    case ManifestUpdateJobResult::kIconWriteToDiskFailed:
      os << "kIconWriteToDiskFailed";
      break;
    case ManifestUpdateJobResult::kInstallFinalizeFailed:
      os << "kInstallFinalizeFailed";
      break;
    case ManifestUpdateJobResult::kManifestConversionFailed:
      os << "kManifestConversionFailed";
      break;
    case ManifestUpdateJobResult::kAppNotAllowedToUpdate:
      os << "kAppNotAllowedToUpdate";
      break;
    case ManifestUpdateJobResult::kWebContentsDestroyed:
      os << "kWebContentsDestroyed";
      break;
    case ManifestUpdateJobResult::kUserNavigated:
      os << "kUserNavigated";
      break;
    case ManifestUpdateJobResult::kSilentlyUpdatedDueToSmallIconComparison:
      os << "kSilentlyUpdatedDueToSmallIconComparison";
      break;
  }
  return os;
}

ManifestUpdateJobResultWithTimestamp::ManifestUpdateJobResultWithTimestamp(
    ManifestUpdateJobResult result,
    std::optional<base::Time> time_for_icon_diff_check)
    : result_(result), time_for_icon_diff_check_(time_for_icon_diff_check) {}
ManifestUpdateJobResultWithTimestamp::ManifestUpdateJobResultWithTimestamp(
    const ManifestUpdateJobResultWithTimestamp&) = default;
ManifestUpdateJobResultWithTimestamp&
ManifestUpdateJobResultWithTimestamp::operator=(
    const ManifestUpdateJobResultWithTimestamp&) = default;
ManifestUpdateJobResultWithTimestamp::ManifestUpdateJobResultWithTimestamp(
    ManifestUpdateJobResultWithTimestamp&&) = default;
ManifestUpdateJobResultWithTimestamp&
ManifestUpdateJobResultWithTimestamp::operator=(
    ManifestUpdateJobResultWithTimestamp&&) = default;
ManifestUpdateJobResultWithTimestamp::~ManifestUpdateJobResultWithTimestamp() =
    default;

std::string ManifestUpdateJobResultWithTimestamp::ToString() const {
  return ToValue().DebugString();
}

base::DictValue ManifestUpdateJobResultWithTimestamp::ToValue() const {
  return base::DictValue()
      .Set("result", base::ToString(result_))
      .Set("time_for_icon_diff_check",
           time_for_icon_diff_check_.has_value()
               ? base::TimeFormatAsIso8601(time_for_icon_diff_check_.value())
               : "null");
}

}  // namespace web_app
