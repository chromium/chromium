// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

class LinkCapturingMigrationManagerBrowserTest : public InProcessBrowserTest {
 public:
  LinkCapturingMigrationManagerBrowserTest() = default;
  ~LinkCapturingMigrationManagerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(browser()->profile()));
  }

 protected:
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_supress_;
};

IN_PROC_BROWSER_TEST_F(LinkCapturingMigrationManagerBrowserTest,
                       MigrateCaptureLinks) {
  Profile* profile = browser()->profile();
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->title = u"Test app";
  web_app_info->start_url = GURL("https://example.org");
  web_app_info->capture_links =
      blink::mojom::CaptureLinks::kExistingClientNavigate;
  AppId app_id = test::InstallWebApp(profile, std::move(web_app_info));
  proxy->FlushMojoCallsForTesting();

  EXPECT_EQ(proxy->PreferredApps().FindPreferredAppForUrl(
                GURL("https://example.org/some/path")),
            app_id);
}

}  // namespace web_app
