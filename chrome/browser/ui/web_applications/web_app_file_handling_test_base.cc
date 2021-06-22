// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_file_handling_test_base.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

namespace web_app {

// static
base::FilePath WebAppFileHandlingTestBase::NewTestFilePath(
    const base::StringPiece extension) {
  // CreateTemporaryFile blocks, temporarily allow blocking.
  base::ScopedAllowBlockingForTesting allow_blocking;

  // In order to test file handling, we need to be able to supply a file
  // extension for the temp file.
  base::FilePath test_file_path;
  base::CreateTemporaryFile(&test_file_path);
  base::FilePath new_file_path = test_file_path.AddExtensionASCII(extension);
  EXPECT_TRUE(base::ReplaceFile(test_file_path, new_file_path, nullptr));
  return new_file_path;
}

// static
content::WebContents* WebAppFileHandlingTestBase::LaunchApplication(
    Profile* profile,
    const std::string& app_id,
    const GURL& expected_launch_url,
    const apps::mojom::LaunchContainer launch_container,
    const apps::mojom::AppLaunchSource launch_source,
    const std::vector<base::FilePath>& files) {
  apps::AppLaunchParams params(app_id, launch_container,
                               WindowOpenDisposition::NEW_WINDOW,
                               launch_source);

  if (files.size())
    params.launch_files = files;

  content::TestNavigationObserver navigation_observer(expected_launch_url);
  navigation_observer.StartWatchingNewWebContents();

  content::WebContents* web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParams(std::move(params));

  navigation_observer.Wait();

  // Attach the launchParams to the window so we can inspect them easily.
  auto result = content::EvalJs(web_contents,
                                "launchQueue.setConsumer(launchParams => {"
                                "  window.launchParams = launchParams;"
                                "});");

  return web_contents;
}

WebAppProviderBase* WebAppFileHandlingTestBase::provider() {
  return WebAppProviderBase::GetProviderBase(profile());
}

FileHandlerManager& WebAppFileHandlingTestBase::file_handler_manager() {
  return provider()
      ->os_integration_manager()
      .file_handler_manager_for_testing();
}

AppRegistrar& WebAppFileHandlingTestBase::registrar() {
  return provider()->registrar();
}

GURL WebAppFileHandlingTestBase::GetSecureAppURL() {
  return https_server()->GetURL("app.com", "/ssl/google.html");
}

GURL WebAppFileHandlingTestBase::GetTextFileHandlerActionURL() {
  return https_server()->GetURL("app.com", "/ssl/blank_page.html");
}

GURL WebAppFileHandlingTestBase::GetCSVFileHandlerActionURL() {
  return https_server()->GetURL("app.com", "/ssl/page_with_refs.html");
}

GURL WebAppFileHandlingTestBase::GetHTMLFileHandlerActionURL() {
  return https_server()->GetURL("app.com", "/ssl/page_with_frame.html");
}

void WebAppFileHandlingTestBase::InstallFileHandlingPWA() {
  GURL url = GetSecureAppURL();

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = url;
  web_app_info->scope = url.GetWithoutFilename();
  web_app_info->title = u"A Hosted App";

  // Basic plain text format.
  blink::Manifest::FileHandler entry1;
  entry1.action = GetTextFileHandlerActionURL();
  entry1.name = u"text";
  entry1.accept[u"text/*"].push_back(u".txt");
  web_app_info->file_handlers.push_back(std::move(entry1));

  // A format that the browser is also a handler for, to confirm that the
  // browser doesn't override PWAs using File Handling for types that the
  // browser also handles.
  blink::Manifest::FileHandler entry2;
  entry2.action = GetHTMLFileHandlerActionURL();
  entry2.name = u"html";
  entry2.accept[u"text/html"].push_back(u".html");
  web_app_info->file_handlers.push_back(std::move(entry2));

  // application/* format.
  blink::Manifest::FileHandler entry3;
  entry3.action = GetCSVFileHandlerActionURL();
  entry3.name = u"csv";
  entry3.accept[u"application/csv"].push_back(u".csv");
  web_app_info->file_handlers.push_back(std::move(entry3));

  app_id_ = WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));
}

void WebAppFileHandlingTestBase::InstallAnotherFileHandlingPwa(
    const GURL& url) {
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = url;
  web_app_info->scope = url.GetWithoutFilename();
  web_app_info->title = u"A second app";

  // This one handles jpegs.
  blink::Manifest::FileHandler entry1;
  entry1.action = GetTextFileHandlerActionURL();
  entry1.name = u"jpeg";
  entry1.accept[u"image/jpeg"].push_back(u".jpeg");
  web_app_info->file_handlers.push_back(std::move(entry1));

  WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));
}

void WebAppFileHandlingTestBase::LaunchWithFiles(
    const std::string& app_id,
    const GURL& expected_launch_url,
    const std::vector<base::FilePath>& files,
    const apps::mojom::LaunchContainer launch_container) {
  web_contents_ = LaunchApplication(
      profile(), app_id, expected_launch_url, launch_container,
      apps::mojom::AppLaunchSource::kSourceFileHandler, files);
}

void WebAppFileHandlingTestBase::VerifyPwaDidReceiveFileLaunchParams(
    bool expect_got_launch_params,
    const base::FilePath& expected_file_path) {
  bool got_launch_params =
      content::EvalJs(web_contents_, "!!window.launchParams").ExtractBool();
  ASSERT_EQ(expect_got_launch_params, got_launch_params);
  if (got_launch_params) {
    EXPECT_EQ(
        1, content::EvalJs(web_contents_, "window.launchParams.files.length"));
    EXPECT_EQ(
        expected_file_path.BaseName().AsUTF8Unsafe(),
        content::EvalJs(web_contents_, "window.launchParams.files[0].name"));
  }
}

ContentSetting WebAppFileHandlingTestBase::GetFileHandlingPermission(
    const GURL& url) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  return map->GetContentSetting(url, url, ContentSettingsType::FILE_HANDLING);
}

void WebAppFileHandlingTestBase::SetFileHandlingPermission(
    ContentSetting setting) {
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetDefaultContentSetting(ContentSettingsType::FILE_HANDLING, setting);
}

}  // namespace web_app
