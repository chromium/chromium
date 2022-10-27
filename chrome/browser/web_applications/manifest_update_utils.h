// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_UTILS_H_

#include <iosfwd>
#include <string>

#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"

namespace web_app {

class WebAppRegistrar;

// This enum is recorded by UMA, the numeric values must not change.
enum ManifestUpdateResult {
  kNoAppInScope = 0,
  kThrottled = 1,
  kWebContentsDestroyed = 2,
  kAppUninstalling = 3,
  kAppIsPlaceholder = 4,
  kAppUpToDate = 5,
  kAppNotEligible = 6,
  kAppUpdateFailed = 7,
  kAppUpdated = 8,
  kAppIsSystemWebApp = 9,
  kIconDownloadFailed = 10,
  kIconReadFromDiskFailed = 11,
  kAppIdMismatch = 12,
  kAppAssociationsUpdateFailed = 13,
  kAppAssociationsUpdated = 14,
  kMaxValue = kAppAssociationsUpdated,
};

std::ostream& operator<<(std::ostream& os, ManifestUpdateResult result);

enum ManifestUpdateStage {
  kPendingInstallableData = 0,
  kPendingIconDownload = 1,
  kPendingIconReadFromDisk = 2,
  kPendingAppIdentityCheck = 3,
  kPendingMaybeReadExistingIcons = 4,
  kPendingAssociationsUpdate = 5,
  kAppWindowsClosed = 6,
  kPendingFinalizerUpdate = 7,
};

std::ostream& operator<<(std::ostream& os, ManifestUpdateStage stage);

// Some apps, such as pre-installed apps, have been vetted and are therefore
// considered safe and permitted to update their names.
bool AllowUnpromptedNameUpdate(const AppId& app_id,
                               const WebAppRegistrar& registrar);

bool NeedsAppIdentityUpdateDialog(bool title_changing,
                                  bool icons_changing,
                                  const AppId& app_id,
                                  const WebAppRegistrar& registrar);

// Checks if a manifest update is required by reading the web_app fields and
// comparing it with the passed install_info.
bool IsUpdateNeededForManifest(const AppId& app_id,
                               const WebAppInstallInfo& install_info,
                               const WebAppRegistrar& registrar);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MANIFEST_UPDATE_UTILS_H_
