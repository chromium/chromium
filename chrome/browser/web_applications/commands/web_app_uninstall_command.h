// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_UNINSTALL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_UNINSTALL_COMMAND_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class Profile;
class PrefService;

namespace webapps {
enum class UninstallResultCode;
enum class WebappUninstallSource;
}  // namespace webapps

namespace web_app {

class AppLock;
class OsIntegrationManager;
class WebAppIconManager;
class WebAppInstallManager;
class WebAppInstallFinalizer;
class WebAppRegistrar;
class WebAppSyncBridge;
class WebAppTranslationManager;

// Uninstall the web app.
class WebAppUninstallCommand : public WebAppCommand {
 public:
  using UninstallWebAppCallback =
      base::OnceCallback<void(webapps::UninstallResultCode)>;

  WebAppUninstallCommand(const AppId& app_id,
                         const url::Origin& app_origin,
                         Profile* profile,
                         OsIntegrationManager* os_integration_manager,
                         WebAppSyncBridge* sync_bridge,
                         WebAppIconManager* icon_manager,
                         WebAppRegistrar* registrar,
                         WebAppInstallManager* install_manager,
                         WebAppInstallFinalizer* install_finalizer,
                         WebAppTranslationManager* translation_manager,
                         webapps::WebappUninstallSource source,
                         UninstallWebAppCallback callback);
  ~WebAppUninstallCommand() override;

  Lock& lock() const override;

  void Start() override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;

  base::Value ToDebugValue() const override;

 private:
  void Abort(webapps::UninstallResultCode code);
  void OnSubAppUninstalled(webapps::UninstallResultCode code);
  void OnOsHooksUninstalled(OsHooksErrors errors);
  void OnIconDataDeleted(bool success);
  void OnTranslationDataDeleted(bool success);
  void MaybeFinishUninstall();

  enum class State {
    kNotStarted = 0,
    kPendingDataDeletion = 1,
    kDone = 2,
  } state_ = State::kNotStarted;

  std::unique_ptr<AppLock> lock_;
  AppId app_id_;
  url::Origin app_origin_;
  webapps::WebappUninstallSource source_;
  UninstallWebAppCallback callback_;

  raw_ptr<OsIntegrationManager> os_integration_manager_;
  raw_ptr<WebAppSyncBridge> sync_bridge_;
  raw_ptr<WebAppIconManager> icon_manager_;
  raw_ptr<WebAppRegistrar> registrar_;
  raw_ptr<WebAppInstallManager> install_manager_;
  raw_ptr<WebAppInstallFinalizer> install_finalizer_;
  raw_ptr<WebAppTranslationManager> translation_manager_;
  raw_ptr<PrefService> profile_prefs_;

  size_t num_pending_sub_app_uninstalls_;

  bool app_data_deleted_ = false;
  bool translation_data_deleted_ = false;
  bool hooks_uninstalled_ = false;
  bool errors_ = false;

  base::WeakPtrFactory<WebAppUninstallCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_WEB_APP_INSTALL_COMMAND_H_
