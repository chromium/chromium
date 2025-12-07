// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_UPDATE_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_UPDATE_RESULT_H_

#include <iosfwd>

#include "base/functional/callback_forward.h"

namespace web_app {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FetchManifestAndUpdateResult {
  kSuccess = 0,
  kSuccessNoUpdateDetected = 1,
  kShutdown = 2,
  kAppNotInstalled = 3,
  kUrlLoadingError = 4,
  kManifestRetrievalError = 5,
  kInvalidManifest = 6,
  kIconDownloadError = 7,
  kInstallationError = 8,
  kPrimaryPageChanged = 9,
  kManifestToWebAppInstallInfoFailed = 10,
  kMaxValue = kManifestToWebAppInstallInfoFailed
};
using FetchManifestAndUpdateCallback =
    base::OnceCallback<void(FetchManifestAndUpdateResult)>;

std::ostream& operator<<(std::ostream& os, FetchManifestAndUpdateResult);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_UPDATE_RESULT_H_
