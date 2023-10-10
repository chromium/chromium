// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/check_isolated_web_app_bundle_installability_command.h"

#include <utility>

#include "base/functional/callback_forward.h"
#include "base/strings/strcat.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_dev_mode.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

CheckIsolatedWebAppBundleInstallabilityCommand::
    CheckIsolatedWebAppBundleInstallabilityCommand(
        Profile* profile,
        const SignedWebBundleMetadata& bundle_metadata,
        BundleInstallabilityCallback callback)
    : WebAppCommandTemplate<AppLock>(
          "CheckIsolatedWebAppBundleInstallabilityCommand"),
      lock_description_(
          std::make_unique<AppLockDescription>(bundle_metadata.app_id())),
      profile_(profile),
      bundle_metadata_(bundle_metadata),
      callback_(std::move(callback)) {
  CHECK(profile_);
  CHECK(!callback_.is_null());
  debug_log_.Set("bundle app ID:", bundle_metadata.app_id());
  debug_log_.Set("bundle app version:", bundle_metadata.version().GetString());
}

CheckIsolatedWebAppBundleInstallabilityCommand::
    ~CheckIsolatedWebAppBundleInstallabilityCommand() = default;

const LockDescription&
CheckIsolatedWebAppBundleInstallabilityCommand::lock_description() const {
  return *lock_description_;
}

base::Value CheckIsolatedWebAppBundleInstallabilityCommand::ToDebugValue()
    const {
  return base::Value(debug_log_.Clone());
}

void CheckIsolatedWebAppBundleInstallabilityCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  const WebApp* app = lock_->registrar().GetAppById(bundle_metadata_.app_id());

  if (!app) {
    ReportResult(InstallabilityCheckResult::kInstallable, absl::nullopt);
    return;
  }

  const absl::optional<WebApp::IsolationData>& isolation_data =
      app->isolation_data();
  // If there is an app with the same app ID, it must be an IWA.
  CHECK(isolation_data.has_value());

  base::Version installed_version = isolation_data.value().version;
  bool is_dev_mode_install = IsIwaDevModeEnabled(profile_);

  if (is_dev_mode_install && bundle_metadata_.version() < installed_version) {
    ReportResult(InstallabilityCheckResult::kOutdated, installed_version);
    return;
  }

  if (!is_dev_mode_install && bundle_metadata_.version() <= installed_version) {
    ReportResult(InstallabilityCheckResult::kOutdated, installed_version);
    return;
  }

  ReportResult(InstallabilityCheckResult::kUpdatable, installed_version);
}

void CheckIsolatedWebAppBundleInstallabilityCommand::OnShutdown() {
  ReportResult(InstallabilityCheckResult::kShutdown, absl::nullopt);
}

void CheckIsolatedWebAppBundleInstallabilityCommand::ReportResult(
    InstallabilityCheckResult status,
    absl::optional<base::Version> installed_version) {
  CHECK(!callback_.is_null());
  std::string message;
  switch (status) {
    case InstallabilityCheckResult::kInstallable:
      message = "Success: Bundle is installable.";
      break;
    case InstallabilityCheckResult::kUpdatable:
      message = base::StrCat({"Success: Bundle contains an update.",
                              "\nVersion of the app inside the bundle: ",
                              bundle_metadata_.version().GetString(),
                              "\nVersion of the app already installed: ",
                              installed_version->GetString()});
      break;
    case InstallabilityCheckResult::kOutdated:
      message = base::StrCat(
          {"Failure: Bundle contains an app with outdated version.",
           "\nVersion of the app inside the bundle: ",
           bundle_metadata_.version().GetString(),
           "\nVersion of the app already installed: ",
           installed_version->GetString()});
      break;
    case InstallabilityCheckResult::kShutdown:
      message = "Shutdown.";
      break;
  }

  debug_log_.Set("result", message);
  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(callback_), status, installed_version));
}

}  // namespace web_app
