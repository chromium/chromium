// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_FILE_HANDLING_TEST_BASE_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_FILE_HANDLING_TEST_BASE_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class AppRegistrar;
class FileHandlerManager;
class WebAppProviderBase;

// Base class for browser tests that cover the File Handling API. See
// //chrome/browser/web_applications/docs/file_handling.md.
class WebAppFileHandlingTestBase : public WebAppControllerBrowserTest {
 public:
  static base::FilePath NewTestFilePath(const base::StringPiece extension);

  // Launches the |app_id| web app with |files| handles, awaits for
  // |expected_launch_url| to load and stashes any launch params on
  // "window.launchParams" for further inspection.
  static content::WebContents* LaunchApplication(
      Profile* profile,
      const std::string& app_id,
      const GURL& expected_launch_url,
      const apps::mojom::LaunchContainer launch_container =
          apps::mojom::LaunchContainer::kLaunchContainerWindow,
      const apps::mojom::AppLaunchSource launch_source =
          apps::mojom::AppLaunchSource::kSourceTest,
      const std::vector<base::FilePath>& files = std::vector<base::FilePath>());

  WebAppProviderBase* provider();
  FileHandlerManager& file_handler_manager();
  AppRegistrar& registrar();

  GURL GetSecureAppURL();
  GURL GetTextFileHandlerActionURL();
  GURL GetCSVFileHandlerActionURL();
  GURL GetHTMLFileHandlerActionURL();

  void InstallFileHandlingPWA();

  void InstallAnotherFileHandlingPwa(const GURL& url);

  void LaunchWithFiles(
      const std::string& app_id,
      const GURL& expected_launch_url,
      const std::vector<base::FilePath>& files,
      const apps::mojom::LaunchContainer launch_container =
          apps::mojom::LaunchContainer::kLaunchContainerWindow);

  void VerifyPwaDidReceiveFileLaunchParams(
      bool expect_got_launch_params,
      const base::FilePath& expected_file_path = {});

  ContentSetting GetFileHandlingPermission(const GURL& url);
  void SetFileHandlingPermission(ContentSetting setting);

  const AppId& app_id() { return app_id_; }

 private:
  AppId app_id_;
  content::WebContents* web_contents_ = nullptr;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_FILE_HANDLING_TEST_BASE_H_
