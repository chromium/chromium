// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_UPDATE_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_UPDATE_RESULT_H_

#include <iosfwd>

#include "base/functional/callback_forward.h"

namespace web_app {

enum class FetchManifestAndUpdateResult {
  kSuccess = 0,
  kShutdown = 1,
  kAppNotInstalled = 2,
  kUrlLoadingError = 3,
  kManifestRetrievalError = 4,
  kInvalidManifest = 5,
  kIconDownloadError = 6,
  kInstallationError = 7,
  kPrimaryPageChanged = 8,
  kMaxValue = kPrimaryPageChanged
};
using FetchManifestAndUpdateCallback =
    base::OnceCallback<void(FetchManifestAndUpdateResult)>;

std::ostream& operator<<(std::ostream& os, FetchManifestAndUpdateResult);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_FETCH_MANIFEST_AND_UPDATE_RESULT_H_
