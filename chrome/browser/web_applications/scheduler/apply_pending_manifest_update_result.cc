// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/scheduler/apply_pending_manifest_update_result.h"

#include <ostream>

namespace web_app {

std::ostream& operator<<(std::ostream& os,
                         ApplyPendingManifestUpdateResult result) {
  switch (result) {
    case ApplyPendingManifestUpdateResult::kSystemShutdown:
      return os << "kSystemShutdown";
    case ApplyPendingManifestUpdateResult::kAppNotInstalled:
      return os << "kAppNotInstalled";
    case ApplyPendingManifestUpdateResult::kIconChangeAppliedSuccessfully:
      return os << "kIconChangeAppliedSuccessfully";
    case ApplyPendingManifestUpdateResult::
        kFailedToOverwriteIconsFromPendingIcons:
      return os << "kFailedToOverwriteIconsFromPendingIcons";
    case ApplyPendingManifestUpdateResult::kNoPendingUpdate:
      return os << "kNoPendingUpdate";
    case ApplyPendingManifestUpdateResult::kFailedToRemovePendingIconsFromDisk:
      return os << "kFailedToRemovePendingIconsFromDisk";
    case ApplyPendingManifestUpdateResult::kAppNameUpdatedSuccessfully:
      return os << "kAppNameUpdatedSuccessfully";
    case ApplyPendingManifestUpdateResult::kAppNameAndIconsUpdatedSuccessfully:
      return os << "kAppNameAndIconsUpdatedSuccessfully";
  }
}

}  // namespace web_app
