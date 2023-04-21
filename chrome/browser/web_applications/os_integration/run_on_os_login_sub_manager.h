// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_RUN_ON_OS_LOGIN_SUB_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_RUN_ON_OS_LOGIN_SUB_MANAGER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace web_app {

class WebAppRegistrar;
class WebAppSyncBridge;
class WebAppIconManager;

class RunOnOsLoginSubManager : public OsIntegrationSubManager {
 public:
  RunOnOsLoginSubManager(Profile& profile,
                         WebAppRegistrar& registrar,
                         WebAppSyncBridge& sync_bridge,
                         WebAppIconManager& icon_manager);
  ~RunOnOsLoginSubManager() override;

  void Configure(const AppId& app_id,
                 proto::WebAppOsIntegrationState& desired_state,
                 base::OnceClosure configure_done) override;

  void Execute(const AppId& app_id,
               const absl::optional<SynchronizeOsOptions>& synchronize_options,
               const proto::WebAppOsIntegrationState& desired_state,
               const proto::WebAppOsIntegrationState& current_state,
               base::OnceClosure execute_done) override;

  void ForceUnregister(const AppId& app_id,
                       base::OnceClosure callback) override;

 private:
  // Unregistration logic.
  void StartUnregistration(const AppId& app_id,
                           const proto::WebAppOsIntegrationState& current_state,
                           const proto::WebAppOsIntegrationState& desired_state,
                           base::OnceClosure registration_callback);

  // Registration logic.
  void CreateShortcutInfoWithFavicons(
      const AppId& app_id,
      const proto::WebAppOsIntegrationState& desired_state,
      base::OnceClosure execute_done);
  void OnShortcutInfoCreatedStartRegistration(
      const AppId& app_id,
      const proto::WebAppOsIntegrationState& desired_state,
      base::OnceClosure execute_done,
      std::unique_ptr<ShortcutInfo> shortcut_info);

  const raw_ref<Profile> profile_;
  const raw_ref<WebAppRegistrar> registrar_;
  const raw_ref<WebAppSyncBridge> sync_bridge_;
  const raw_ref<WebAppIconManager> icon_manager_;

  base::WeakPtrFactory<RunOnOsLoginSubManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_RUN_ON_OS_LOGIN_SUB_MANAGER_H_
