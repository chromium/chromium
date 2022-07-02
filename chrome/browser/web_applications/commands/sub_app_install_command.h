// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_SUB_APP_INSTALL_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_SUB_APP_INSTALL_COMMAND_H_

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/subapps/sub_apps_service.mojom-shared.h"

using AppInstallResults = std::vector<
    std::pair<web_app::AppId, blink::mojom::SubAppsServiceAddResultCode>>;

class SubAppInstallCommand : public web_app::WebAppCommand {
 public:
  SubAppInstallCommand(
      web_app::WebAppInstallManager* install_manager,
      web_app::WebAppRegistrar* registrar,
      web_app::AppId& parent_app_id,
      std::vector<std::pair<web_app::UnhashedAppId, GURL>> sub_apps,
      base::flat_set<web_app::AppId> app_ids_for_lock,
      base::OnceCallback<void(AppInstallResults)> callback);
  ~SubAppInstallCommand() override;
  SubAppInstallCommand(const SubAppInstallCommand&) = delete;
  SubAppInstallCommand& operator=(const SubAppInstallCommand&) = delete;
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
      const web_app::UnhashedAppId& app_id,
      content::WebContents* initiator_web_contents,
      std::unique_ptr<WebAppInstallInfo> web_app_info,
      web_app::WebAppInstallationAcceptanceCallback acceptance_callback);

  void MaybeShowDialog();

  void OnInstalled(const web_app::UnhashedAppId& unhashed_app_id,
                   const web_app::AppId& app_id,
                   webapps::InstallResultCode result);

  void MaybeFinishCommand();

  void AddResultAndRemoveFromPendingInstalls(
      const web_app::UnhashedAppId& unhashed_app_id,
      webapps::InstallResultCode result);

  raw_ptr<web_app::WebAppInstallManager> install_manager_;
  raw_ptr<web_app::WebAppRegistrar> registrar_;
  std::vector<std::pair<web_app::UnhashedAppId, GURL>> requested_installs_;
  std::set<web_app::UnhashedAppId> pending_installs_;
  size_t num_pending_dialog_callbacks_ = 0;
  AppInstallResults results_;
  const web_app::AppId parent_app_id_;
  base::OnceCallback<void(AppInstallResults)> install_callback_;

  std::vector<
      std::tuple<web_app::UnhashedAppId,
                 std::unique_ptr<WebAppInstallInfo>,
                 base::OnceCallback<void(bool user_accepted,
                                         std::unique_ptr<WebAppInstallInfo>)>>>
      acceptance_callbacks_;
  std::vector<std::pair<std::u16string, SkBitmap>> dialog_data_;
  base::WeakPtrFactory<SubAppInstallCommand> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_SUB_APP_INSTALL_COMMAND_H_
