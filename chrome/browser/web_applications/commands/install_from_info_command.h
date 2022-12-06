// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_INFO_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_INFO_COMMAND_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;

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
class InstallFromInfoCommand : public WebAppCommandTemplate<AppLock> {
 public:
  InstallFromInfoCommand(std::unique_ptr<WebAppInstallInfo> install_info,
                         bool overwrite_existing_manifest_fields,
                         webapps::WebappInstallSource install_surface,
                         OnceInstallCallback install_callback);

  InstallFromInfoCommand(std::unique_ptr<WebAppInstallInfo> install_info,
                         bool overwrite_existing_manifest_fields,
                         webapps::WebappInstallSource install_surface,
                         OnceInstallCallback install_callback,
                         const WebAppInstallParams& install_params);

  ~InstallFromInfoCommand() override;

  LockDescription& lock_description() const override;

  void StartWithLock(std::unique_ptr<AppLock> lock) override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;

  base::Value ToDebugValue() const override;

 private:
  void PopulateInitialDebugInfo();

  void Abort(webapps::InstallResultCode code);

  void OnInstallCompleted(const AppId& app_id,
                          webapps::InstallResultCode code,
                          OsHooksErrors os_hooks_errors);

  std::unique_ptr<AppLockDescription> lock_description_;
  std::unique_ptr<AppLock> lock_;

  AppId app_id_;
  std::unique_ptr<WebAppInstallInfo> install_info_;
  bool overwrite_existing_manifest_fields_;
  webapps::WebappInstallSource install_surface_;
  OnceInstallCallback install_callback_;
  absl::optional<WebAppInstallParams> install_params_;

  base::Value::Dict debug_value_;

  base::WeakPtrFactory<InstallFromInfoCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_INFO_COMMAND_H_
