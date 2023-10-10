// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHECK_ISOLATED_WEB_APP_BUNDLE_INSTALLABILITY_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHECK_ISOLATED_WEB_APP_BUNDLE_INSTALLABILITY_COMMAND_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/version.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"

class Profile;

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;

// Checks the registrar for Isolated Web App installability given the
// |bundle_metadata|.
class CheckIsolatedWebAppBundleInstallabilityCommand
    : public WebAppCommandTemplate<AppLock> {
 public:
  enum class InstallabilityCheckResult {
    kInstallable,
    // The app inside the bundle is already installed, but the bundle contains
    // an updated version.
    kUpdatable,
    // The app inside the bundle is already installed, and the bundle contains
    // an outdated version.
    kOutdated,
    kShutdown,
  };

  // If an app with the same app ID is already installed, runs the callback with
  // the version of the installed app. Otherwise, |installed_version| will be
  // null.
  using BundleInstallabilityCallback =
      base::OnceCallback<void(InstallabilityCheckResult result,
                              absl::optional<base::Version> installed_version)>;

  CheckIsolatedWebAppBundleInstallabilityCommand(
      Profile* profile,
      const SignedWebBundleMetadata& bundle_metadata,
      BundleInstallabilityCallback callback);
  ~CheckIsolatedWebAppBundleInstallabilityCommand() override;

  // WebAppCommandTemplate<AppLock>:
  const LockDescription& lock_description() const override;
  base::Value ToDebugValue() const override;
  void StartWithLock(std::unique_ptr<AppLock> lock) override;
  void OnShutdown() override;

 private:
  void ReportResult(InstallabilityCheckResult status,
                    absl::optional<base::Version> installed_version);

  base::Value::Dict debug_log_;
  std::unique_ptr<AppLockDescription> lock_description_;
  std::unique_ptr<AppLock> lock_;

  raw_ptr<Profile> profile_;
  SignedWebBundleMetadata bundle_metadata_;
  BundleInstallabilityCallback callback_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHECK_ISOLATED_WEB_APP_BUNDLE_INSTALLABILITY_COMMAND_H_
