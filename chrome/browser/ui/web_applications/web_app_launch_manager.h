// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Browser;
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
  using OpenApplicationCallback = base::RepeatingCallback<content::WebContents*(
      apps::AppLaunchParams&& params)>;

  explicit WebAppLaunchManager(Profile* profile);
  WebAppLaunchManager(const WebAppLaunchManager&) = delete;
  WebAppLaunchManager& operator=(const WebAppLaunchManager&) = delete;
  virtual ~WebAppLaunchManager();

  content::WebContents* OpenApplication(apps::AppLaunchParams&& params);

  // |browser| may be nullptr if the navigation fails.
  void LaunchApplication(
      const std::string& app_id,
      const base::CommandLine& command_line,
      const base::FilePath& current_directory,
      const absl::optional<GURL>& url_handler_launch_url,
      const absl::optional<GURL>& protocol_handler_launch_url,
      const absl::optional<GURL>& file_launch_url,
      const std::vector<base::FilePath>& launch_files,
      base::OnceCallback<void(Browser* browser,
                              apps::LaunchContainer container)> callback);

  static void SetOpenApplicationCallbackForTesting(
      OpenApplicationCallback callback);

  // Created temporarily while this class is migrated to the command system.
  static OpenApplicationCallback& GetOpenApplicationCallbackForTesting();

 private:
  virtual void LaunchWebApplication(
      apps::AppLaunchParams&& params,
      base::OnceCallback<void(Browser* browser,
                              apps::LaunchContainer container)> callback);

  const raw_ptr<Profile> profile_;
  const raw_ptr<WebAppProvider> provider_;

  base::WeakPtrFactory<WebAppLaunchManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_MANAGER_H_
