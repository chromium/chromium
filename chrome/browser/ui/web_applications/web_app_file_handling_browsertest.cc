// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/common/web_application_info.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "third_party/blink/public/common/features.h"

class WebAppFileHandlingBrowserTest
    : public web_app::WebAppControllerBrowserTest {
 public:
  WebAppFileHandlingBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kNativeFileSystemAPI,
         blink::features::kFileHandlingAPI},
        {});
  }

  GURL GetSecureAppURL() {
    return https_server()->GetURL("app.com", "/ssl/google.html");
  }

  GURL GetFileHandlerActionURL() {
    return https_server()->GetURL("app.com", "/ssl/blank_page.html");
  }

  base::FilePath NewTestFilePath() {
    // CreateTemporaryFile blocks, temporarily allow blocking.
    base::ScopedAllowBlockingForTesting allow_blocking;

    base::FilePath test_file_path;
    base::CreateTemporaryFile(&test_file_path);
    return test_file_path;
  }

  std::string InstallFileHandlingPWA() {
    GURL url = GetSecureAppURL();

    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->app_url = url;
    web_app_info->scope = url.GetWithoutFilename();
    web_app_info->title = base::ASCIIToUTF16("A Hosted App");
    web_app_info->file_handler = blink::Manifest::FileHandler();
    web_app_info->file_handler->action = GetFileHandlerActionURL();

    {
      std::vector<blink::Manifest::FileFilter> filters;
      blink::Manifest::FileFilter text = {
          base::ASCIIToUTF16("text"),
          {base::ASCIIToUTF16(".txt"), base::ASCIIToUTF16("text/*")}};
      filters.push_back(text);
      web_app_info->file_handler->files = std::move(filters);
    }

    return WebAppControllerBrowserTest::InstallWebApp(std::move(web_app_info));
  }

  content::WebContents* LaunchWithFiles(
      const std::string& app_id,
      const std::vector<base::FilePath>& files) {
    apps::AppLaunchParams params(
        app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW,
        apps::mojom::AppLaunchSource::kSourceFileHandler);
    params.launch_files = files;

    content::TestNavigationObserver navigation_observer(
        GetFileHandlerActionURL());
    navigation_observer.StartWatchingNewWebContents();

    params.override_url = GetFileHandlerActionURL();
    content::WebContents* web_contents =
        apps::LaunchService::Get(profile())->OpenApplication(params);

    navigation_observer.Wait();

    // Attach the launchParams to the window so we can inspect them easily.
    auto result = content::EvalJs(web_contents,
                                  "launchQueue.setConsumer(launchParams => {"
                                  "  window.launchParams = launchParams;"
                                  "});");

    return web_contents;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest, PWAsCanViewLaunchParams) {
  ASSERT_TRUE(https_server()->Start());

  const std::string app_id = InstallFileHandlingPWA();
  content::WebContents* web_contents = LaunchWithFiles(app_id, {});
  EXPECT_EQ(false, content::EvalJs(web_contents, "!!window.launchParams"));
}

IN_PROC_BROWSER_TEST_P(WebAppFileHandlingBrowserTest,
                       PWAsCanReceiveFileLaunchParams) {
  ASSERT_TRUE(https_server()->Start());

  const std::string app_id = InstallFileHandlingPWA();
  base::FilePath test_file_path = NewTestFilePath();
  content::WebContents* web_contents =
      LaunchWithFiles(app_id, {test_file_path});

  EXPECT_EQ(1,
            content::EvalJs(web_contents, "window.launchParams.files.length"));
  EXPECT_EQ(test_file_path.BaseName().value(),
            content::EvalJs(web_contents, "window.launchParams.files[0].name"));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebAppFileHandlingBrowserTest,
    ::testing::Values(
        web_app::ControllerType::kHostedAppController,
        web_app::ControllerType::kUnifiedControllerWithBookmarkApp,
        web_app::ControllerType::kUnifiedControllerWithWebApp),
    web_app::ControllerTypeParamToString);
