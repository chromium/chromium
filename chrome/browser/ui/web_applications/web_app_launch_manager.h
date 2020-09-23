// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Browser;
enum class WindowOpenDisposition;
class GURL;
class Profile;

namespace apps {
struct AppLaunchParams;
}  // namespace apps

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

class WebAppProvider;

// Handles launch requests for Desktop PWAs and bookmark apps.
// Web applications have type AppType::kWeb in the app registry.
class WebAppLaunchManager {
 public:
  explicit WebAppLaunchManager(Profile* profile);
  ~WebAppLaunchManager();

  // apps::LaunchManager:
  content::WebContents* OpenApplication(const apps::AppLaunchParams& params);

  void LaunchApplication(
      const std::string& app_id,
      const base::CommandLine& command_line,
      const base::FilePath& current_directory,
      base::OnceCallback<void(Browser* browser,
                              apps::mojom::LaunchContainer container)>
          callback);

 private:
  void LaunchWebApplication(
      apps::AppLaunchParams params,
      base::OnceCallback<void(Browser* browser,
                              apps::mojom::LaunchContainer container)>
          callback);

  Profile* const profile_;
  WebAppProvider* const provider_;

  base::WeakPtrFactory<WebAppLaunchManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebAppLaunchManager);
};

Browser* CreateWebApplicationWindow(Profile* profile,
                                    const std::string& app_id,
                                    WindowOpenDisposition disposition,
                                    bool can_resize = true);

content::WebContents* NavigateWebApplicationWindow(
    Browser* browser,
    const std::string& app_id,
    const GURL& url,
    WindowOpenDisposition disposition);

void RecordAppWindowLaunch(Profile* profile, const std::string& app_id);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_
