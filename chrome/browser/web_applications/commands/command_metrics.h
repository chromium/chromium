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
enum class InstallCommand {
  kFetchManifestAndInstall,
  kInstallAppFromVerifiedManifest,
  kInstallFromInfo,
  kInstallIsolatedWebApp,
  kWebAppInstallFromUrl,
};

// These values must be kept in sync with the variant list in
// .../webapps/histograms.xml.
enum class WebAppType {
  kCraftedApp,
  kDiyApp,
  // Not enough information is known to determine if the app would have been
  // crafted or diy.
  kUnknown,
  kIsolatedWebApp,
};

void RecordInstallMetrics(InstallCommand command,
                          WebAppType app_type,
                          webapps::InstallResultCode result,
                          webapps::WebappInstallSource source);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_COMMAND_METRICS_H_
