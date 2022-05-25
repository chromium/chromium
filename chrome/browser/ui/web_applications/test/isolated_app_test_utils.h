// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_APP_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_APP_TEST_UTILS_H_

#include <string>

#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class GURL;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace web_app {

class IsolatedAppBrowserTestHarness : public WebAppControllerBrowserTest {
 public:
  IsolatedAppBrowserTestHarness();
  IsolatedAppBrowserTestHarness(const IsolatedAppBrowserTestHarness&) = delete;
  IsolatedAppBrowserTestHarness& operator=(
      const IsolatedAppBrowserTestHarness&) = delete;
  ~IsolatedAppBrowserTestHarness() override;

 protected:
  AppId InstallIsolatedApp(const std::string& host);
  AppId InstallIsolatedApp(const GURL& app_url);
  content::RenderFrameHost* OpenApp(const AppId& app_id);
  content::RenderFrameHost* NavigateToURLInNewTab(
      Browser* window,
      const GURL& url,
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB);

  Browser* GetBrowserFromFrame(content::RenderFrameHost* frame);
  void CreateIframe(content::RenderFrameHost* parent_frame,
                    const std::string& iframe_id,
                    const GURL& url,
                    const std::string& permissions_policy);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_APP_TEST_UTILS_H_
