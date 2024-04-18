// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_INFO_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_INFO_COMMAND_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class Profile;

namespace web_app {

class InstallFromInfoJob;
struct WebAppInstallParams;

// Starts a web app installation process using prefilled
// |install_info| which holds all the data needed for installation.
// This doesn't fetch a manifest and doesn't perform all required steps for
// External installed apps: use |ExternallyManagedAppManager::Install|
// instead.
//
// The web app can be simultaneously installed from multiple sources.
// If the web app already exists and `overwrite_existing_manifest_fields` is
// false then manifest fields in `install_info` are treated only as
// fallback manifest values. If `overwrite_existing_manifest_fields` is true
// then the existing web app manifest fields will be overwritten.
// If `install_info` contains data freshly fetched from the web app's
// site then `overwrite_existing_manifest_fields` should be true.
class InstallFromInfoCommand
    : public WebAppCommand<AppLock,
                           const webapps::AppId&,
                           webapps::InstallResultCode> {
 public:
  // The `install_params` controls whether and how OS hooks get installed. By
  // default, no params means no os hooks.
  InstallFromInfoCommand(Profile* profile,
                         std::unique_ptr<WebAppInstallInfo> install_info,
                         bool overwrite_existing_manifest_fields,
                         webapps::WebappInstallSource install_surface,
                         OnceInstallCallback install_callback,
                         std::optional<WebAppInstallParams> install_params);

  ~InstallFromInfoCommand() override;

  // WebAppCommand:
  void OnShutdown(base::PassKey<WebAppCommandManager>) const override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  void OnInstallFromInfoJobCompleted(webapps::AppId app_id,
                                     webapps::InstallResultCode code);

  raw_ref<Profile> profile_;

  const webapps::AppId app_id_;

  std::unique_ptr<AppLock> lock_;

  std::unique_ptr<InstallFromInfoJob> install_from_info_job_;

  base::WeakPtrFactory<InstallFromInfoCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_INFO_COMMAND_H_
