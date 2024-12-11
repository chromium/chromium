// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/command_metrics.h"

#include <string_view>

#include "base/metrics/histogram_functions_internal_overloads.h"
#include "base/strings/strcat.h"

namespace web_app {
namespace {
constexpr std::string_view ToVariantString(InstallCommand command) {
  // This must exactly match the string in .../webapps/histograms.xml.
  switch (command) {
    case InstallCommand::kFetchManifestAndInstall:
      return ".FetchManifestAndInstall";
    case InstallCommand::kInstallAppFromVerifiedManifest:
      return ".InstallAppFromVerifiedManifest";
    case InstallCommand::kInstallFromInfo:
      return ".InstallFromInfo";
    case InstallCommand::kInstallIsolatedWebApp:
      return ".InstallIsolatedWebApp";
    case InstallCommand::kWebAppInstallFromUrl:
      return ".WebInstallFromUrl";
  }
}

constexpr std::string_view ToVariantString(WebAppType type) {
  // This must exactly match the string in .../webapps/histograms.xml.
  switch (type) {
    case WebAppType::kCraftedApp:
      return ".CraftedApp";
    case WebAppType::kDiyApp:
      return ".DiyApp";
    case WebAppType::kUnknown:
      return ".Unknown";
    case WebAppType::kIsolatedWebApp:
      return ".IsolatedWebApp";
  }
}
}  // namespace

void RecordInstallMetrics(InstallCommand command,
                          WebAppType app_type,
                          webapps::InstallResultCode result,
                          webapps::WebappInstallSource source) {
  base::UmaHistogramEnumeration("WebApp.InstallCommand.ResultCode", result);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"WebApp.InstallCommand", ToVariantString(command), ".ResultCode"}),
      result);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"WebApp.InstallCommand", ToVariantString(app_type), ".ResultCode"}),
      result);
  base::UmaHistogramEnumeration(
      base::StrCat({"WebApp.InstallCommand", ToVariantString(command),
                    ToVariantString(app_type), ".ResultCode"}),
      result);

  base::UmaHistogramEnumeration("WebApp.InstallCommand.Surface", source,
                                webapps::WebappInstallSource::COUNT);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"WebApp.InstallCommand", ToVariantString(command), ".Surface"}),
      source, webapps::WebappInstallSource::COUNT);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"WebApp.InstallCommand", ToVariantString(app_type), ".Surface"}),
      source, webapps::WebappInstallSource::COUNT);
  base::UmaHistogramEnumeration(
      base::StrCat({"WebApp.InstallCommand", ToVariantString(command),
                    ToVariantString(app_type), ".Surface"}),
      source, webapps::WebappInstallSource::COUNT);
}

}  // namespace web_app
