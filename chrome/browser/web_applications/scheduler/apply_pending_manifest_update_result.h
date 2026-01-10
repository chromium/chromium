// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_APPLY_PENDING_MANIFEST_UPDATE_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_APPLY_PENDING_MANIFEST_UPDATE_RESULT_H_

#include <iosfwd>

#include "base/functional/callback_forward.h"

namespace web_app {

// This enum is recorded by UMA, the numeric values must not change.
// LINT.IfChange(ApplyPendingManifestUpdateResult)
enum class ApplyPendingManifestUpdateResult {
  kSystemShutdown = 0,
  kAppNotInstalled = 1,
  kIconChangeAppliedSuccessfully = 2,
  kFailedToOverwriteIconsFromPendingIcons = 3,
  kNoPendingUpdate = 4,
  kFailedToRemovePendingIconsFromDisk = 5,
  kAppNameUpdatedSuccessfully = 6,
  kAppNameAndIconsUpdatedSuccessfully = 7,
  kMaxValue = kAppNameAndIconsUpdatedSuccessfully
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:WebAppApplyPendingManifestUpdateResult)

std::ostream& operator<<(std::ostream& os,
                         ApplyPendingManifestUpdateResult stage);

using ApplyPendingManifestUpdateCompletedCallback =
    base::OnceCallback<void(ApplyPendingManifestUpdateResult update_result)>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_APPLY_PENDING_MANIFEST_UPDATE_RESULT_H_
