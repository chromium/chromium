// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "net/dns/mock_host_resolver.h"

namespace web_app {

std::string ControllerTypeParamToString(
    const ::testing::TestParamInfo<ControllerType>& controller_type) {
  switch (controller_type.param) {
    case ControllerType::kHostedAppController:
      return "HostedAppController";
    case ControllerType::kUnifiedControllerWithBookmarkApp:
      return "UnifiedControllerWithBookmarkApp";
    case ControllerType::kUnifiedControllerWithWebApp:
      return "UnifiedControllerWithWebApp";
  }
}

WebAppControllerBrowserTestBase::WebAppControllerBrowserTestBase() {
  if (GetParam() == ControllerType::kUnifiedControllerWithWebApp) {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAsWithoutExtensions}, {});
  } else if (GetParam() == ControllerType::kUnifiedControllerWithBookmarkApp) {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAsUnifiedUiController},
        {features::kDesktopPWAsWithoutExtensions});
  } else {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kDesktopPWAsUnifiedUiController,
             features::kDesktopPWAsWithoutExtensions});
  }
}

WebAppControllerBrowserTestBase::~WebAppControllerBrowserTestBase() = default;

AppId WebAppControllerBrowserTestBase::InstallPWA(const GURL& app_url) {
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = app_url;
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->open_as_window = true;
  return web_app::InstallWebApp(profile(), std::move(web_app_info));
}

AppId WebAppControllerBrowserTestBase::InstallWebApp(
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  return web_app::InstallWebApp(profile(), std::move(web_app_info));
}

Browser* WebAppControllerBrowserTestBase::LaunchWebAppBrowser(
    const AppId& app_id) {
  return web_app::LaunchWebAppBrowser(profile(), app_id);
}

Browser* WebAppControllerBrowserTestBase::LaunchBrowserForWebAppInTab(
    const AppId& app_id) {
  return web_app::LaunchBrowserForWebAppInTab(profile(), app_id);
}

base::Optional<AppId> WebAppControllerBrowserTestBase::FindAppWithUrlInScope(
    const GURL& url) {
  auto* provider = WebAppProvider::Get(profile());
  DCHECK(provider);
  return provider->registrar().FindAppWithUrlInScope(url);
}

WebAppControllerBrowserTest::WebAppControllerBrowserTest()
    : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
  scoped_feature_list_.InitWithFeatures(
      {}, {predictors::kSpeculativePreconnectFeature});
}

WebAppControllerBrowserTest::~WebAppControllerBrowserTest() = default;

void WebAppControllerBrowserTest::SetUp() {
  https_server_.AddDefaultHandlers(GetChromeTestDataDir());

  extensions::ExtensionBrowserTest::SetUp();
}

content::WebContents* WebAppControllerBrowserTest::OpenApplication(
    const AppId& app_id) {
  auto* provider = WebAppProvider::Get(profile());
  DCHECK(provider);
  ui_test_utils::UrlLoadObserver url_observer(
      provider->registrar().GetAppLaunchURL(app_id),
      content::NotificationService::AllSources());

  apps::AppLaunchParams params(
      app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW,
      apps::mojom::AppLaunchSource::kSourceTest);
  content::WebContents* contents =
      apps::LaunchService::Get(profile())->OpenApplication(params);
  url_observer.Wait();
  return contents;
}

void WebAppControllerBrowserTest::SetUpInProcessBrowserTestFixture() {
  extensions::ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
  cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void WebAppControllerBrowserTest::TearDownInProcessBrowserTestFixture() {
  extensions::ExtensionBrowserTest::TearDownInProcessBrowserTestFixture();
  cert_verifier_.TearDownInProcessBrowserTestFixture();
}

void WebAppControllerBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
  cert_verifier_.SetUpCommandLine(command_line);
}

void WebAppControllerBrowserTest::SetUpOnMainThread() {
  extensions::ExtensionBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");

  // By default, all SSL cert checks are valid. Can be overridden in tests.
  cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
}

}  // namespace web_app
