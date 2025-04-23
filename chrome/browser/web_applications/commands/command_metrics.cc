// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/command_metrics.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace web_app {
namespace {
constexpr std::string_view ToVariantString(InstallCommand command) {
  // These must exactly match the variant strings in .../webapps/histograms.xml.
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
  // These must exactly match the variant strings in .../webapps/histograms.xml.
  switch (type) {
    case WebAppType::kCraftedApp:
      return ".Crafted";
    case WebAppType::kDiyApp:
      return ".Diy";
    case WebAppType::kUnknown:
      return ".Unknown";
    case WebAppType::kIsolatedWebApp:
      return ".Isolated";
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

  base::UmaHistogramEnumeration("WebApp.InstallCommand.Surface", source);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"WebApp.InstallCommand", ToVariantString(command), ".Surface"}),
      source);
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"WebApp.InstallCommand", ToVariantString(app_type), ".Surface"}),
      source);
  base::UmaHistogramEnumeration(
      base::StrCat({"WebApp.InstallCommand", ToVariantString(command),
                    ToVariantString(app_type), ".Surface"}),
      source);
}

}  // namespace web_app
