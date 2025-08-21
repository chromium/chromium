// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_CHECK_ISOLATED_WEB_APP_BUNDLE_INSTALLABILITY_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_CHECK_ISOLATED_WEB_APP_BUNDLE_INSTALLABILITY_COMMAND_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"

class Profile;

namespace web_app {

// The result of checking if a Signed Web Bundle is installable as an Isolated
// Web App.
enum class IsolatedInstallabilityCheckResult {
  // The bundle is installable as a new app.
  kInstallable,
  // The app inside the bundle is already installed, but the bundle contains
  // an updated version.
  kUpdatable,
  // The app inside the bundle is already installed, and the bundle contains
  // an outdated version.
  kOutdated,
  // The system was shut down before the command could complete.
  kShutdown,
};

// Checks if a Signed Web Bundle is installable as an Isolated Web App. It
// compares the version from the bundle's metadata with the version of an
// already installed app (if one exists) to determine if the bundle is a new
// install, an update, or outdated.
class CheckIsolatedWebAppBundleInstallabilityCommand
    : public WebAppCommand<AppLock,
                           IsolatedInstallabilityCheckResult,
                           std::optional<IwaVersion>> {
 public:
  // If an app with the same app ID is already installed, runs the callback with
  // the version of the installed app. Otherwise, |installed_version| will be
  // null.
  using BundleInstallabilityCallback =
      base::OnceCallback<void(IsolatedInstallabilityCheckResult result,
                              std::optional<IwaVersion> installed_version)>;

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
                    std::optional<IwaVersion> installed_version);

  std::unique_ptr<AppLock> lock_;

  raw_ptr<Profile> profile_;
  SignedWebBundleMetadata bundle_metadata_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_CHECK_ISOLATED_WEB_APP_BUNDLE_INSTALLABILITY_COMMAND_H_
