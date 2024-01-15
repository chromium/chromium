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
#include "chrome/browser/web_applications/locks/app_lock.h"

class Profile;

namespace web_app {

enum class IsolatedInstallabilityCheckResult {
  kInstallable,
  // The app inside the bundle is already installed, but the bundle contains
  // an updated version.
  kUpdatable,
  // The app inside the bundle is already installed, and the bundle contains
  // an outdated version.
  kOutdated,
  kShutdown,
};

// Checks the registrar for Isolated Web App installability given the
// |bundle_metadata|.
class CheckIsolatedWebAppBundleInstallabilityCommand
    : public WebAppCommand<AppLock,
                           IsolatedInstallabilityCheckResult,
                           std::optional<base::Version>> {
 public:
  // If an app with the same app ID is already installed, runs the callback with
  // the version of the installed app. Otherwise, |installed_version| will be
  // null.
  using BundleInstallabilityCallback =
      base::OnceCallback<void(IsolatedInstallabilityCheckResult result,
                              std::optional<base::Version> installed_version)>;

  CheckIsolatedWebAppBundleInstallabilityCommand(
      Profile* profile,
      const SignedWebBundleMetadata& bundle_metadata,
      BundleInstallabilityCallback callback);
  ~CheckIsolatedWebAppBundleInstallabilityCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  void ReportResult(IsolatedInstallabilityCheckResult status,
                    std::optional<base::Version> installed_version);

  std::unique_ptr<AppLock> lock_;

  raw_ptr<Profile> profile_;
  SignedWebBundleMetadata bundle_metadata_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHECK_ISOLATED_WEB_APP_BUNDLE_INSTALLABILITY_COMMAND_H_
