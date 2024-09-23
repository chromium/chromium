// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/permission_bubble/permission_bubble_browser_test_util.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"

PermissionBubbleBrowserTest::PermissionBubbleBrowserTest() = default;

PermissionBubbleBrowserTest::~PermissionBubbleBrowserTest() = default;

void PermissionBubbleBrowserTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();

  // Add a single permission request.
  requests_.push_back(std::make_unique<permissions::MockPermissionRequest>(
      permissions::RequestType::kNotifications));

  std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
      raw_requests;
  raw_requests.push_back(requests_[0].get());
  test_delegate_.set_requests(raw_requests);
}

content::WebContents* PermissionBubbleBrowserTest::OpenExtensionAppWindow() {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app_with_panel_container/"));
  CHECK(extension);

  apps::AppLaunchParams params(
      extension->id(), apps::LaunchContainer::kLaunchContainerPanelDeprecated,
      WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);

  content::WebContents* app_contents =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(std::move(params));
  CHECK(app_contents);
  return app_contents;
}

PermissionBubbleKioskBrowserTest::PermissionBubbleKioskBrowserTest() {
}

PermissionBubbleKioskBrowserTest::~PermissionBubbleKioskBrowserTest() {
}

void PermissionBubbleKioskBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PermissionBubbleBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kKioskMode);
  // Navigate to a test file URL.
  GURL test_file_url(ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("simple.html"))));
  command_line->AppendArg(test_file_url.spec());
}
