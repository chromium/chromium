// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_INFO_AND_REPLACE_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_INFO_AND_REPLACE_COMMAND_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class Profile;

namespace web_app {

class InstallFromInfoJob;
class WebAppUninstallAndReplaceJob;
struct WebAppInstallInfo;
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
//
// This command also uninstalls other apps and extensions, and applies
// configurations of the first replaced on on the installed app.
class InstallFromInfoAndReplaceCommand
    : public WebAppCommand<AppLock,
                           const webapps::AppId&,
                           webapps::InstallResultCode,
                           bool /*did_uninstall_and_replace*/> {
 public:
  using InstallAndReplaceCallback =
      base::OnceCallback<void(const webapps::AppId& app_id,
                              webapps::InstallResultCode code,
                              bool did_uninstall_and_replace)>;

  // The `install_params` controls whether and how OS hooks get installed.
  InstallFromInfoAndReplaceCommand(
      Profile* profile,
      std::unique_ptr<WebAppInstallInfo> install_info,
      bool overwrite_existing_manifest_fields,
      webapps::WebappInstallSource install_surface,
      InstallAndReplaceCallback install_callback,
      const WebAppInstallParams& install_params,
      const std::vector<webapps::AppId>& apps_or_extensions_to_uninstall);

  ~InstallFromInfoAndReplaceCommand() override;

  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

  void OnInstallFromInfoJobCompleted(webapps::AppId app_id,
                                     webapps::InstallResultCode code,
                                     OsHooksErrors os_hook_errors);
  void OnUninstallAndReplaced(webapps::InstallResultCode code,
                              bool did_uninstall_and_replace);

 private:
  void Abort(webapps::InstallResultCode code);

  raw_ref<Profile> profile_;

  webapps::ManifestId manifest_id_;
  webapps::AppId app_id_;
  std::vector<webapps::AppId> apps_or_extensions_to_uninstall_;

  std::unique_ptr<AppLock> lock_;

  std::unique_ptr<InstallFromInfoJob> install_from_info_job_;
  std::unique_ptr<WebAppUninstallAndReplaceJob> uninstall_and_replace_job_;

  base::WeakPtrFactory<InstallFromInfoAndReplaceCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_INFO_AND_REPLACE_COMMAND_H_
