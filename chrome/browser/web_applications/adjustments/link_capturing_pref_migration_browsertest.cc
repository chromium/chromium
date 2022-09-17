// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

class LinkCapturingPrefMigrationBrowserTest : public InProcessBrowserTest {
 public:
  LinkCapturingPrefMigrationBrowserTest() = default;
  ~LinkCapturingPrefMigrationBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    test::WaitUntilReady(WebAppProvider::GetForTest(browser()->profile()));
  }

 protected:
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_supress_;
};

IN_PROC_BROWSER_TEST_F(LinkCapturingPrefMigrationBrowserTest,
                       MigrateCaptureLinks) {
  Profile* profile = browser()->profile();
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);

  auto web_app_info = std::make_unique<WebAppInstallInfo>();
  web_app_info->title = u"Test app";
  web_app_info->start_url = GURL("https://example.org");
  web_app_info->capture_links =
      blink::mojom::CaptureLinks::kExistingClientNavigate;
  AppId app_id = test::InstallWebApp(profile, std::move(web_app_info));

  EXPECT_EQ(proxy->PreferredAppsList().FindPreferredAppForUrl(
                GURL("https://example.org/some/path")),
            app_id);
}

}  // namespace web_app
