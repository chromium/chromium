// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMMAND_METRICS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMMAND_METRICS_H_

#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace web_app {

// These values must be kept in sync with the variant list in
// .../webapps/histograms.xml.
// LINT.IfChange(InstallCommand)
enum class InstallCommand {
  // A user-triggered installation from an install surface in the browser.
  kFetchManifestAndInstall,
  // An installation triggered by the Web App Pre-install system.
  kInstallAppFromVerifiedManifest,
  // A programmatic installation using pre-filled `WebAppInstallInfo`.
  kInstallFromInfo,
  // An installation of an Isolated Web App.
  kInstallIsolatedWebApp,
  // An installation triggered by the `navigator.install()` Web API.
  kWebAppInstallFromUrl,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/histograms.xml)

// These values must be kept in sync with the variant list in
// .../webapps/histograms.xml.
// LINT.IfChange(WebAppType)
enum class WebAppType {
  // An app installed from a site that meets all PWA criteria.
  kCraftedApp,
  // An app installed from a site that does not meet all PWA criteria, where
  // metadata is synthesized by the browser.
  kDiyApp,
  // Not enough information is known to determine if the app would have been
  // crafted or diy.
  kUnknown,
  // An Isolated Web App.
  kIsolatedWebApp,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/histograms.xml)

void RecordInstallMetrics(InstallCommand command,
                          WebAppType app_type,
                          webapps::InstallResultCode result,
                          webapps::WebappInstallSource source);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMMAND_METRICS_H_
