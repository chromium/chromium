// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_SUB_APP_INSTALL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_SUB_APP_INSTALL_COMMAND_H_

#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "components/webapps/browser/install_result_code.h"
#include "third_party/blink/public/mojom/subapps/sub_apps_service.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

struct WebAppInstallInfo;

namespace content {
class WebContents;
}

namespace web_app {

class AppLock;
class WebAppInstallManager;
class WebAppRegistrar;

using AppInstallResults =
    std::vector<std::pair<AppId, blink::mojom::SubAppsServiceAddResultCode>>;

class SubAppInstallCommand : public WebAppCommand {
 public:
  SubAppInstallCommand(WebAppInstallManager* install_manager,
                       WebAppRegistrar* registrar,
                       AppId& parent_app_id,
                       std::vector<std::pair<UnhashedAppId, GURL>> sub_apps,
                       base::flat_set<AppId> app_ids_for_lock,
                       base::OnceCallback<void(AppInstallResults)> callback);
  ~SubAppInstallCommand() override;
  SubAppInstallCommand(const SubAppInstallCommand&) = delete;
  SubAppInstallCommand& operator=(const SubAppInstallCommand&) = delete;

  Lock& lock() const override;

  base::Value ToDebugValue() const override;

 protected:
  void Start() override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;

 private:
  enum class State {
    kNotStarted = 0,
    kPendingDialogCallbacks = 1,
    kPendingInstallComplete = 2
  } state_ = State::kNotStarted;

  void StartNextInstall();

  void OnDialogRequested(
      const UnhashedAppId& app_id,
      content::WebContents* initiator_web_contents,
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      WebAppInstallationAcceptanceCallback acceptance_callback);

  void MaybeShowDialog();

  void OnInstalled(const UnhashedAppId& unhashed_app_id,
                   const AppId& app_id,
                   webapps::InstallResultCode result);

  void MaybeFinishCommand();

  void AddResultAndRemoveFromPendingInstalls(
      const UnhashedAppId& unhashed_app_id,
      webapps::InstallResultCode result);

  std::unique_ptr<AppLock> lock_;
  raw_ptr<WebAppInstallManager> install_manager_;
  raw_ptr<WebAppRegistrar> registrar_;
  std::vector<std::pair<UnhashedAppId, GURL>> requested_installs_;
  std::set<UnhashedAppId> pending_installs_;
  size_t num_pending_dialog_callbacks_ = 0;
  AppInstallResults results_;
  const AppId parent_app_id_;
  base::OnceCallback<void(AppInstallResults)> install_callback_;

  std::vector<
      std::tuple<UnhashedAppId,
                 std::unique_ptr<WebAppInstallInfo>,
                 base::OnceCallback<void(bool user_accepted,
                                         std::unique_ptr<WebAppInstallInfo>)>>>
      acceptance_callbacks_;
  std::vector<std::pair<std::u16string, SkBitmap>> dialog_data_;
  base::WeakPtrFactory<SubAppInstallCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_SUB_APP_INSTALL_COMMAND_H_
