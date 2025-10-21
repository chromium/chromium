// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_manifest_and_update_result.h"

#include <ostream>

namespace web_app {

std::ostream& operator<<(std::ostream& os,
                         FetchManifestAndUpdateResult result) {
  switch (result) {
    case FetchManifestAndUpdateResult::kSuccess:
      return os << "kSuccess";
    case FetchManifestAndUpdateResult::kSuccessNoUpdateDetected:
      return os << "kSuccessNoUpdateDetected";
    case FetchManifestAndUpdateResult::kShutdown:
      return os << "kShutdown";
    case FetchManifestAndUpdateResult::kAppNotInstalled:
      return os << "kAppNotInstalled";
    case FetchManifestAndUpdateResult::kUrlLoadingError:
      return os << "kUrlLoadingError";
    case FetchManifestAndUpdateResult::kManifestRetrievalError:
      return os << "kManifestRetrievalError";
    case FetchManifestAndUpdateResult::kInvalidManifest:
      return os << "kInvalidManifest";
    case FetchManifestAndUpdateResult::kIconDownloadError:
      return os << "kIconDownloadError";
    case FetchManifestAndUpdateResult::kInstallationError:
      return os << "kInstallationError";
    case FetchManifestAndUpdateResult::kPrimaryPageChanged:
      return os << "kPrimaryPageChanged";
    case FetchManifestAndUpdateResult::kManifestToWebAppInstallInfoFailed:
      return os << "kManifestToWebAppInstallInfoFailed";
  }
}

}  // namespace web_app
