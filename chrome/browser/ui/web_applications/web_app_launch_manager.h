// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/launch_service/launch_manager.h"

namespace apps {
struct AppLaunchParams;
}  // namespace apps

namespace web_app {

class WebAppProvider;

// Handles launch requests for Desktop PWAs and bookmark apps.
// Web applications have type AppType::kWeb in the app registry.
class WebAppLaunchManager : public apps::LaunchManager {
 public:
  explicit WebAppLaunchManager(Profile* profile);
  ~WebAppLaunchManager() override;

  // apps::LaunchManager:
  content::WebContents* OpenApplication(
      const apps::AppLaunchParams& params) override;

  bool OpenApplicationWindow(const std::string& app_id,
                             const base::CommandLine& command_line,
                             const base::FilePath& current_directory) override;

  bool OpenApplicationTab(const std::string& app_id) override;

 private:
  void OpenWebApplication(const apps::AppLaunchParams& params);

  WebAppProvider* const provider_;

  base::WeakPtrFactory<WebAppLaunchManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppLaunchManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_
