// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_RUN_ON_OS_LOGIN_SUB_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_RUN_ON_OS_LOGIN_SUB_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

class WebAppRegistrar;

class RunOnOsLoginSubManager : public OsIntegrationSubManager {
 public:
  explicit RunOnOsLoginSubManager(WebAppRegistrar& registrar);
  ~RunOnOsLoginSubManager() override;
  void Start() override;
  void Shutdown() override;

  void Configure(const AppId& app_id,
                 proto::WebAppOsIntegrationState& desired_state,
                 base::OnceClosure configure_done) override;

  void Execute(const AppId& app_id,
               const absl::optional<SynchronizeOsOptions>& synchronize_options,
               const proto::WebAppOsIntegrationState& desired_state,
               const proto::WebAppOsIntegrationState& current_state,
               base::OnceClosure callback) override;

 private:
  const raw_ref<WebAppRegistrar> registrar_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_RUN_ON_OS_LOGIN_SUB_MANAGER_H_
