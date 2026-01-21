// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_FETCH_INSTALL_INFO_FROM_INSTALL_URL_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_FETCH_INSTALL_INFO_FROM_INSTALL_URL_RESULT_H_

#include <iosfwd>
#include <memory>

#include "base/functional/callback_forward.h"

namespace web_app {

struct WebAppInstallInfo;

// The result of fetching the `WebAppInstallInfo` from an `install_url`.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(FetchInstallInfoResult)
enum class FetchInstallInfoResult {
  // Successfully fetched the `WebAppInstallInfo`.
  kAppInfoObtained = 0,
  // The web contents was destroyed before the command could complete.
  kWebContentsWasDestroyed = 1,
  // The given `install_url` failed to load.
  kUrlLoadingFailure = 2,
  // The site did not have a valid web app manifest.
  kNoValidManifest = 3,
  // The manifest ID of the fetched manifest did not match the expected ID.
  kWrongManifestId = 4,
  // A generic failure occurred.
  kFailure = 5,
  // The system was shut down.
  kShutdown = 6,
  kMaxValue = kShutdown
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:FetchInstallInfoResult)

std::ostream& operator<<(std::ostream& os, FetchInstallInfoResult result);

using FetchInstallInfoFromInstallUrlCallback =
    base::OnceCallback<void(std::unique_ptr<WebAppInstallInfo> install_info)>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_FETCH_INSTALL_INFO_FROM_INSTALL_URL_RESULT_H_
