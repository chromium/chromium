// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_METRICS_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_METRICS_HELPER_H_

#include <utility>
#include <vector>

#include "chrome/browser/web_applications/scheduler/apply_pending_manifest_update_result.h"
#include "chrome/browser/web_applications/scheduler/manifest_silent_update_result.h"
#include "url/origin.h"

namespace web_app {

class WebAppRegistrar;

class IsolatedWebAppMetricsHelper {
 public:
  // This enum is recorded by UMA, the numeric values must not change.
  // LINT.IfChange(LogSubAppInstallResult)
  enum class LogSubAppInstallResult {
    kSuccess = 0,
    kSuccessBypassApproval = 1,
    kFailureGeneral = 2,
    kFailureInvalidManifest = 3,
    kFailureAlreadyInstalled = 4,
    kFailureUserDeclined = 5,
    kFailureUserDeclinedEmbargo = 6,
    kFailureNumberOfSubAppsExceedsLimit = 7,
    kFailureOverlappingScope = 8,
    kFailureRecursiveInstall = 9,
    kFailureWebAppsNotUserInstallable = 10
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:SubAppInstallServiceResult)

  IsolatedWebAppMetricsHelper() = delete;

  // Records the number of installed sub apps for parent IWAs.
  // Recorded on Chrome start.
  static void ReportNumInstalledSubApps(const WebAppRegistrar& registrar);

  // Records the results of multiple sub app installations for the same parent.
  static void ReportSubAppInstallResults(
      const url::Origin& parent_app_origin,
      const std::vector<LogSubAppInstallResult>& results);

  // Records the result of a sub app silent manifest update.
  static void ReportSubAppSilentUpdateResult(
      const url::Origin& parent_app_origin,
      ManifestSilentUpdateCheckResult result);

  // Records the result of a sub app pending manifest update.
  static void ReportSubAppPendingUpdateResult(
      const url::Origin& parent_app_origin,
      ApplyPendingManifestUpdateResult result);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_METRICS_HELPER_H_
