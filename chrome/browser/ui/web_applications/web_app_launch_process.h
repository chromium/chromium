// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_PROCESS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_PROCESS_H_

#include "base/memory/raw_ptr.h"
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

class WebApp;
class WebAppProvider;

// Used by WebAppLaunchManager, this executes an individual launch of a web app
// from any entry point (OS launcher, file handler, protocol handler,
// link capturing, etc.).
//
// Implements the behaviour of the `launch_handler` manifest field:
// https://github.com/WICG/sw-launch/blob/main/launch_handler.md
class WebAppLaunchProcess {
 public:
  WebAppLaunchProcess(Profile& profile, const apps::AppLaunchParams& params);
  WebAppLaunchProcess(const WebAppLaunchProcess&) = delete;

  content::WebContents* Run();

 private:
  const apps::ShareTarget* MaybeGetShareTarget() const;
  std::tuple<GURL, bool /*is_file_handling*/> GetLaunchUrl(
      const apps::ShareTarget* share_target) const;
  WindowOpenDisposition GetNavigationDisposition(bool is_new_browser) const;
  std::tuple<Browser*, bool /*is_new_browser*/> EnsureBrowser();
  LaunchHandler::ClientMode GetLaunchClientMode() const;
  bool LaunchInExistingClient() const;
  bool NeverNavigateExistingClients() const;

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

  Profile& profile_;
  WebAppProvider& provider_;
  const apps::AppLaunchParams& params_;
  const raw_ptr<const WebApp> web_app_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_PROCESS_H_
