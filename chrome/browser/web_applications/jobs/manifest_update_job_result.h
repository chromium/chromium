// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_MANIFEST_UPDATE_JOB_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_MANIFEST_UPDATE_JOB_RESULT_H_

#include <iosfwd>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace base {
class DictValue;
}  // namespace base

namespace web_app {

enum class ManifestUpdateJobResult {
  kNoUpdateNeeded,
  kSilentlyUpdated,
  kPendingUpdateRecorded_AppOnlyHasSecurityUpdate,
  kPendingUpdateRecorded_AppHasSecurityUpdateDueToThrottle,
  kPendingUpdateRecorded_AppHasNonSecurityAndSecurityChanges,
  kIconDownloadFailed,
  kIconReadFromDiskFailed,
  kIconWriteToDiskFailed,
  kInstallFinalizeFailed,
  kManifestConversionFailed,
  kAppNotAllowedToUpdate,
  kWebContentsDestroyed,
  kUserNavigated,
  kSilentlyUpdatedDueToSmallIconComparison,
};

std::ostream& operator<<(std::ostream& os, ManifestUpdateJobResult result);

class ManifestUpdateJobResultWithTimestamp {
 public:
  ManifestUpdateJobResultWithTimestamp(
      ManifestUpdateJobResult result,
      std::optional<base::Time> time_for_icon_diff_check);
  ManifestUpdateJobResultWithTimestamp(
      const ManifestUpdateJobResultWithTimestamp&);
  ManifestUpdateJobResultWithTimestamp& operator=(
      const ManifestUpdateJobResultWithTimestamp&);
  ManifestUpdateJobResultWithTimestamp(ManifestUpdateJobResultWithTimestamp&&);
  ManifestUpdateJobResultWithTimestamp& operator=(
      ManifestUpdateJobResultWithTimestamp&&);
  ~ManifestUpdateJobResultWithTimestamp();

  const ManifestUpdateJobResult& result() const { return result_; }

  // This is the current time, set if the job allowed an icon change to be
  // silently applied due to a small visual difference.
  // This is nullopt otherwise.
  const std::optional<base::Time>& time_for_icon_diff_check() const {
    return time_for_icon_diff_check_;
  }

  std::string ToString() const;
  base::DictValue ToValue() const;

 private:
  ManifestUpdateJobResult result_;
  std::optional<base::Time> time_for_icon_diff_check_;
};

using ManifestUpdateJobCallback =
    base::OnceCallback<void(ManifestUpdateJobResultWithTimestamp result)>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_MANIFEST_UPDATE_JOB_RESULT_H_
