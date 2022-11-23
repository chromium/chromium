// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_SUB_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_SUB_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {

class OsIntegrationSubManager {
 public:
  virtual ~OsIntegrationSubManager() = 0;
  virtual void Start() = 0;
  virtual void Shutdown() = 0;
  virtual void Configure(const AppId& app_id,
                         proto::WebAppOsIntegrationState& desired_state,
                         base::OnceClosure configure_done) = 0;
  virtual void Execute(
      const AppId& app_id,
      const proto::WebAppOsIntegrationState& desired_state,
      const absl::optional<proto::WebAppOsIntegrationState>& current_state,
      base::OnceClosure callback) = 0;
};
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_SUB_MANAGER_H_
