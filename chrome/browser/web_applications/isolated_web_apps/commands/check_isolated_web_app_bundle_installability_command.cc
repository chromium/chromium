// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/check_isolated_web_app_bundle_installability_command.h"

#include <utility>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

CheckIsolatedWebAppBundleInstallabilityCommand::
    CheckIsolatedWebAppBundleInstallabilityCommand(
        Profile* profile,
        const SignedWebBundleMetadata& bundle_metadata,
        BundleInstallabilityCallback callback)
    : WebAppCommand<AppLock,
                    IsolatedInstallabilityCheckResult,
                    std::optional<IwaVersion>>(
          "CheckIsolatedWebAppBundleInstallabilityCommand",
          AppLockDescription(bundle_metadata.app_id()),
          std::move(callback),
          /*args_for_shutdown=*/
          std::make_tuple(IsolatedInstallabilityCheckResult::kShutdown,
                          std::nullopt)),
      profile_(profile),
      bundle_metadata_(bundle_metadata) {
  CHECK(profile_);
  GetMutableDebugValue().Set("bundle app ID:", bundle_metadata.app_id());
  GetMutableDebugValue().Set("bundle app version:",
                             bundle_metadata.version().GetString());
}

CheckIsolatedWebAppBundleInstallabilityCommand::
    ~CheckIsolatedWebAppBundleInstallabilityCommand() = default;

void CheckIsolatedWebAppBundleInstallabilityCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  const WebApp* app = lock_->registrar().GetAppById(bundle_metadata_.app_id());

  if (!app) {
    ReportResult(IsolatedInstallabilityCheckResult::kInstallable, std::nullopt);
    return;
  }

  const std::optional<IsolationData>& isolation_data = app->isolation_data();
  // If there is an app with the same app ID, it must be an IWA.
  CHECK(isolation_data.has_value());

  auto iwa_version = IwaVersion::Create(isolation_data->version().GetString());
  IwaVersion installed_version = *std::move(iwa_version);

  bool is_dev_mode_install = IsIwaDevModeEnabled(profile_);

  if (is_dev_mode_install && bundle_metadata_.version() < installed_version) {
    ReportResult(IsolatedInstallabilityCheckResult::kOutdated,
                 std::move(installed_version));
    return;
  }

  if (!is_dev_mode_install && bundle_metadata_.version() <= installed_version) {
    ReportResult(IsolatedInstallabilityCheckResult::kOutdated,
                 std::move(installed_version));
    return;
  }

  ReportResult(IsolatedInstallabilityCheckResult::kUpdatable,
               std::move(installed_version));
}

void CheckIsolatedWebAppBundleInstallabilityCommand::ReportResult(
    IsolatedInstallabilityCheckResult status,
    std::optional<IwaVersion> installed_version) {
  std::string message;
  bool success = false;
  switch (status) {
    case IsolatedInstallabilityCheckResult::kInstallable:
      message = "Success: Bundle is installable.";
      success = true;
      break;
    case IsolatedInstallabilityCheckResult::kUpdatable:
      message = base::StrCat({"Success: Bundle contains an update.",
                              "\nVersion of the app inside the bundle: ",
                              bundle_metadata_.version().GetString(),
                              "\nVersion of the app already installed: ",
                              installed_version.value().GetString()});
      success = true;
      break;
    case IsolatedInstallabilityCheckResult::kOutdated:
      message = base::StrCat(
          {"Failure: Bundle contains an app with outdated version.",
           "\nVersion of the app inside the bundle: ",
           bundle_metadata_.version().GetString(),
           "\nVersion of the app already installed: ",
           installed_version.value().GetString()});
      break;
    case IsolatedInstallabilityCheckResult::kShutdown:
      NOTREACHED();
  }

  GetMutableDebugValue().Set("result", message);
  CompleteAndSelfDestruct(
      success ? CommandResult::kSuccess : CommandResult::kFailure, status,
      std::move(installed_version));
}

}  // namespace web_app
