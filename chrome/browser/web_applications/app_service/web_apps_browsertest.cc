// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_apps.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_launch_process.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"
#include "url/gurl.h"

namespace web_app {

using WebAppsBrowserTest = WebAppBrowserTestBase;

IN_PROC_BROWSER_TEST_F(WebAppsBrowserTest, LaunchWithIntent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_share_target/charts.html"));
  Profile* const profile = browser()->profile();
  const webapps::AppId app_id = InstallWebAppFromManifest(browser(), app_url);

  base::RunLoop run_loop;
  WebAppLaunchProcess::SetOpenApplicationCallbackForTesting(
      base::BindLambdaForTesting([&run_loop](apps::AppLaunchParams params) {
        EXPECT_EQ(params.intent->action, apps_util::kIntentActionSend);
        EXPECT_EQ(*params.intent->mime_type, "text/csv");
        EXPECT_EQ(params.intent->files.size(), 1U);
        run_loop.Quit();
      }));

  std::vector<base::FilePath> file_paths(
      {ash::CrosDisksClient::GetArchiveMountPoint().Append("numbers.csv")});
  std::vector<std::string> content_types({"text/csv"});
  apps::IntentPtr intent = apps_util::CreateShareIntentFromFiles(
      profile, std::move(file_paths), std::move(content_types));
  const int32_t event_flags =
      apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true);
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
      app_id, event_flags, std::move(intent),
      apps::LaunchSource::kFromSharesheet,
      std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId),
      base::DoNothing());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppsBrowserTest, IntentWithoutFiles) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_share_target/poster.html"));
  Profile* const profile = browser()->profile();
  const webapps::AppId app_id = InstallWebAppFromManifest(browser(), app_url);

  base::RunLoop run_loop;
  WebAppLaunchProcess::SetOpenApplicationCallbackForTesting(
      base::BindLambdaForTesting([&run_loop](apps::AppLaunchParams params) {
        EXPECT_EQ(params.intent->action, apps_util::kIntentActionSendMultiple);
        EXPECT_EQ(*params.intent->mime_type, "*/*");
        EXPECT_EQ(params.intent->files.size(), 0U);
        run_loop.Quit();
      }));

  apps::IntentPtr intent = apps_util::CreateShareIntentFromFiles(
      profile, /*file_paths=*/std::vector<base::FilePath>(),
      /*mime_types=*/std::vector<std::string>(),
      /*share_text=*/"Message",
      /*share_title=*/"Subject");

  const int32_t event_flags =
      apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true);
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
      app_id, event_flags, std::move(intent),
      apps::LaunchSource::kFromSharesheet,
      std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId),
      base::DoNothing());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppsBrowserTest, ExposeAppServicePublisherId) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(embedded_test_server()->GetURL("/web_apps/basic.html"));

  // Install file handling web app.
  const webapps::AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  const WebAppRegistrar& registrar =
      WebAppProvider::GetForTest(browser()->profile())->registrar_unsafe();
  const WebApp* web_app = registrar.GetAppById(app_id);
  ASSERT_TRUE(web_app);

  // Check the publisher_id is the app's start url.
  apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
      ->AppRegistryCache()
      .ForOneApp(app_id, [&](const apps::AppUpdate& update) {
        EXPECT_EQ(web_app->start_url().spec(), update.PublisherId());
      });
}

IN_PROC_BROWSER_TEST_F(WebAppsBrowserTest, LaunchAppIconKeyUnchanged) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(embedded_test_server()->GetURL("/web_apps/basic.html"));
  const webapps::AppId app_id = InstallWebAppFromManifest(browser(), app_url);
  auto* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());

  std::optional<apps::IconKey> original_key;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&original_key](const apps::AppUpdate& update) {
        original_key = update.IconKey();
      });

  const int32_t event_flags =
      apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                          /*prefer_container=*/true);
  proxy->Launch(app_id, event_flags, apps::LaunchSource::kUnknown,
                std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId));

  proxy->AppRegistryCache().ForOneApp(
      app_id, [&original_key](const apps::AppUpdate& update) {
        ASSERT_EQ(original_key, update.IconKey());
      });
}

}  // namespace web_app
