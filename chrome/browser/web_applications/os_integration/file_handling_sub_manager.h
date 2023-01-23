// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_FILE_HANDLING_SUB_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_FILE_HANDLING_SUB_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {

class WebAppRegistrar;

// Used to track updates to the file handlers for a web app.
class FileHandlingSubManager : public OsIntegrationSubManager {
 public:
  explicit FileHandlingSubManager(WebAppRegistrar& registrar);
  ~FileHandlingSubManager() override;
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

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_FILE_HANDLING_SUB_MANAGER_H_
