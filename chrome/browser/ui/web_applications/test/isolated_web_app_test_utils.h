// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_

#include <string>

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class GURL;
class Profile;

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
  IsolatedWebAppUrlInfo InstallDevModeProxyIsolatedWebApp(
      const url::Origin& origin);
  content::RenderFrameHost* OpenApp(const AppId& app_id);
  content::RenderFrameHost* NavigateToURLInNewTab(
      Browser* window,
      const GURL& url,
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB);

  Browser* GetBrowserFromFrame(content::RenderFrameHost* frame);

 private:
  base::test::ScopedFeatureList iwa_scoped_feature_list_;
};

std::unique_ptr<net::EmbeddedTestServer> CreateAndStartDevServer(
    const base::FilePath::StringPieceType& chrome_test_data_relative_root);

IsolatedWebAppUrlInfo InstallDevModeProxyIsolatedWebApp(
    Profile* profile,
    const url::Origin& proxy_origin);

content::RenderFrameHost* OpenIsolatedWebApp(Profile* profile,
                                             const AppId& app_id);

void CreateIframe(content::RenderFrameHost* parent_frame,
                  const std::string& iframe_id,
                  const GURL& url,
                  const std::string& permissions_policy);

// Adds an Isolated Web App to the WebAppRegistrar. The IWA will have an empty
// filepath for |IsolatedWebAppLocation|.
AppId AddDummyIsolatedAppToRegistry(
    Profile* profile,
    const GURL& start_url,
    const std::string& name,
    const WebApp::IsolationData& isolation_data =
        WebApp::IsolationData(InstalledBundle{.path = base::FilePath()},
                              base::Version("1.0.0")));

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_ISOLATED_WEB_APP_TEST_UTILS_H_
