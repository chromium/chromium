// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_RUN_ON_OS_LOGIN_SUB_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_RUN_ON_OS_LOGIN_SUB_MANAGER_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

class WebAppProvider;

class RunOnOsLoginSubManager : public OsIntegrationSubManager {
 public:
  RunOnOsLoginSubManager(Profile& profile, WebAppProvider& provider);
  ~RunOnOsLoginSubManager() override;

  void Configure(const webapps::AppId& app_id,
                 proto::WebAppOsIntegrationState& desired_state,
                 base::OnceClosure configure_done) override;

  void Execute(const webapps::AppId& app_id,
               const std::optional<SynchronizeOsOptions>& synchronize_options,
               const proto::WebAppOsIntegrationState& desired_state,
               const proto::WebAppOsIntegrationState& current_state,
               base::OnceClosure execute_done) override;

  void ForceUnregister(const webapps::AppId& app_id,
                       base::OnceClosure callback) override;

 private:
  // Unregistration logic.
  void StartUnregistration(const webapps::AppId& app_id,
                           const proto::WebAppOsIntegrationState& current_state,
                           const proto::WebAppOsIntegrationState& desired_state,
                           base::OnceClosure registration_callback);

  // Registration logic.
  void CreateShortcutInfoWithFavicons(
      const webapps::AppId& app_id,
      const proto::WebAppOsIntegrationState& desired_state,
      base::OnceClosure execute_done);
  void OnShortcutInfoCreatedStartRegistration(
      const webapps::AppId& app_id,
      const proto::WebAppOsIntegrationState& desired_state,
      base::OnceClosure execute_done,
      std::unique_ptr<ShortcutInfo> shortcut_info);

  const raw_ref<Profile> profile_;
  const raw_ref<WebAppProvider> provider_;

  base::WeakPtrFactory<RunOnOsLoginSubManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_RUN_ON_OS_LOGIN_SUB_MANAGER_H_
