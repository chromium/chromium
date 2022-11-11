// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class GURL;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace net::test_server {
class EmbeddedTestServer;
}  // namespace net::test_server

namespace url {
class Origin;
}  // namespace url

namespace web_app {

class IsolatedWebAppUrlInfo;

class IsolatedWebAppBrowserTestHarness : public WebAppControllerBrowserTest {
 public:
  IsolatedWebAppBrowserTestHarness();
  IsolatedWebAppBrowserTestHarness(const IsolatedWebAppBrowserTestHarness&) =
      delete;
  IsolatedWebAppBrowserTestHarness& operator=(
      const IsolatedWebAppBrowserTestHarness&) = delete;
  ~IsolatedWebAppBrowserTestHarness() override;

 protected:
  std::unique_ptr<net::EmbeddedTestServer> CreateAndStartServer(
      const base::FilePath::StringPieceType& chrome_test_data_relative_root);
  AppId InstallIsolatedWebApp(const std::string& host);
  AppId InstallIsolatedWebApp(const GURL& app_url);
  IsolatedWebAppUrlInfo InstallDevModeProxyIsolatedWebApp(
      const url::Origin& origin);
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

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_
