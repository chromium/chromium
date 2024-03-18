// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_PROCESS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_PROCESS_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "third_party/blink/public/common/manifest/manifest.h"

class Browser;
enum class WindowOpenDisposition;
class GURL;
class Profile;

namespace apps {
struct AppLaunchParams;
struct ShareTarget;
}  // namespace apps

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

class OsIntegrationManager;
class WebApp;
class WebAppRegistrar;

// Used by WebAppLaunchManager, this executes an individual launch of a web app
// from any entry point (OS launcher, file handler, protocol handler,
// link capturing, etc.).
//
// Implements the behaviour of the `launch_handler` manifest field:
// https://github.com/WICG/web-app-launch/blob/main/launch_handler.md
class WebAppLaunchProcess {
 public:
  using OpenApplicationCallback =
      base::RepeatingCallback<void(apps::AppLaunchParams params)>;

  WebAppLaunchProcess(const WebAppLaunchProcess&) = delete;

  static content::WebContents* CreateAndRun(
      Profile& profile,
      WebAppRegistrar& registrar,
      OsIntegrationManager& os_integration_manager,
      const apps::AppLaunchParams& params);

  static void SetOpenApplicationCallbackForTesting(
      OpenApplicationCallback callback);

  // Created temporarily while this class is migrated to the command system.
  static OpenApplicationCallback& GetOpenApplicationCallbackForTesting();

 private:
  WebAppLaunchProcess(Profile& profile,
                      WebAppRegistrar& registrar,
                      OsIntegrationManager& os_integration_manager,
                      const apps::AppLaunchParams& params);
  content::WebContents* Run();

  const apps::ShareTarget* MaybeGetShareTarget() const;
  std::tuple<GURL, bool /*is_file_handling*/> GetLaunchUrl(
      const apps::ShareTarget* share_target) const;
  WindowOpenDisposition GetNavigationDisposition(bool is_new_browser) const;
  std::tuple<Browser*, bool /*is_new_browser*/> EnsureBrowser();
  LaunchHandler GetLaunchHandler() const;
  LaunchHandler::ClientMode GetLaunchClientMode() const;

  // Returns nullptr if these is no existing browser to be used for the launch.
  Browser* MaybeFindBrowserForLaunch() const;
  Browser* CreateBrowserForLaunch();

  struct NavigateResult {
    raw_ptr<content::WebContents> web_contents = nullptr;
    bool did_navigate;
  };
  NavigateResult MaybeNavigateBrowser(Browser* browser,
                                      bool is_new_browser,
                                      const GURL& launch_url,
                                      const apps::ShareTarget* share_target);

  void MaybeEnqueueWebLaunchParams(const GURL& launch_url,
                                   bool is_file_handling,
                                   content::WebContents* web_contents,
                                   bool started_new_navigation);

  const raw_ref<Profile> profile_;
  const raw_ref<WebAppRegistrar> registrar_;
  const raw_ref<OsIntegrationManager> os_integration_manager_;
  const raw_ref<const apps::AppLaunchParams> params_;
  const raw_ptr<const WebApp> web_app_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_PROCESS_H_
