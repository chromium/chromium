// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_ids_map.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

class PendingBookmarkAppManagerBrowserTest : public InProcessBrowserTest {
 protected:
  void InstallApp(const GURL& url,
                  bool bypass_service_worker_check = false,
                  bool require_manifest = false) {
    base::RunLoop run_loop;
    web_app::WebAppProvider::Get(browser()->profile())
        ->pending_app_manager()
        .Install(web_app::PendingAppManager::AppInfo(
                     url, web_app::LaunchContainer::kWindow,
                     web_app::InstallSource::kInternal,
                     false /* create_shortcuts */,  // Avoid creating real
                                                    // shortcuts in tests.
                     web_app::PendingAppManager::AppInfo::
                         kDefaultOverridePreviousUserUninstall,
                     bypass_service_worker_check, require_manifest),
                 base::BindLambdaForTesting(
                     [this, &run_loop](const GURL& provided_url,
                                       web_app::InstallResultCode code) {
                       result_code_ = code;
                       run_loop.QuitClosure().Run();
                     }));
    run_loop.Run();
    ASSERT_TRUE(result_code_.has_value());
  }

  base::Optional<web_app::InstallResultCode> result_code_;
};

// Basic integration test to make sure the whole flow works. Each step in the
// flow is unit tested separately.
IN_PROC_BROWSER_TEST_F(PendingBookmarkAppManagerBrowserTest, InstallSucceeds) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  InstallApp(url);
  EXPECT_EQ(web_app::InstallResultCode::kSuccess, result_code_.value());
  base::Optional<std::string> id =
      web_app::ExtensionIdsMap(browser()->profile()->GetPrefs())
          .LookupExtensionId(url);
  ASSERT_TRUE(id.has_value());
  const Extension* app = ExtensionRegistry::Get(browser()->profile())
                             ->enabled_extensions()
                             .GetByID(id.value());
  ASSERT_TRUE(app);
  EXPECT_EQ("Manifest test app", app->name());
}

// Tests that the browser doesn't crash if it gets shutdown with a pending
// installation.
IN_PROC_BROWSER_TEST_F(PendingBookmarkAppManagerBrowserTest,
                       ShutdownWithPendingInstallation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Start an installation but don't wait for it to finish.
  web_app::WebAppProvider::Get(browser()->profile())
      ->pending_app_manager()
      .Install(web_app::PendingAppManager::AppInfo(
                   embedded_test_server()->GetURL(
                       "/banners/manifest_test_page.html"),
                   web_app::LaunchContainer::kWindow,
                   web_app::InstallSource::kInternal,
                   false /* create_shortcuts */),  // Avoid creating real
                                                   // shortcuts in tests.
               base::DoNothing());

  // The browser should shutdown cleanly even if there is a pending
  // installation.
}

IN_PROC_BROWSER_TEST_F(PendingBookmarkAppManagerBrowserTest,
                       BypassServiceWorkerCheck) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kDesktopPWAWindowing);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html"));
  InstallApp(url, true /* bypass_service_worker_check */);
  const extensions::Extension* app =
      extensions::util::GetInstalledPwaForUrl(browser()->profile(), url);
  EXPECT_TRUE(app);
  EXPECT_EQ("Manifest test app", app->name());
}

IN_PROC_BROWSER_TEST_F(PendingBookmarkAppManagerBrowserTest,
                       PerformServiceWorkerCheck) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kDesktopPWAWindowing);
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html"));
  InstallApp(url, false /* bypass_service_worker_check */);
  const extensions::Extension* app =
      extensions::util::GetInstalledPwaForUrl(browser()->profile(), url);
  EXPECT_FALSE(app);
}

// Test that adding a manifest that points to a chrome:// URL does not actually
// install a bookmark app that points to a chrome:// URL.
IN_PROC_BROWSER_TEST_F(PendingBookmarkAppManagerBrowserTest,
                       InstallChromeURLFails) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest=manifest_chrome_url.json"));
  InstallApp(url);
  EXPECT_EQ(web_app::InstallResultCode::kSuccess, result_code_.value());
  base::Optional<std::string> id =
      web_app::ExtensionIdsMap(browser()->profile()->GetPrefs())
          .LookupExtensionId(url);
  ASSERT_TRUE(id.has_value());
  const Extension* app = ExtensionRegistry::Get(browser()->profile())
                             ->enabled_extensions()
                             .GetByID(id.value());
  ASSERT_TRUE(app);

  // The installer falls back to installing a bookmark app of the original URL.
  EXPECT_EQ(url, extensions::AppLaunchInfo::GetLaunchWebURL(app));
  EXPECT_FALSE(extensions::UrlHandlers::CanBookmarkAppHandleUrl(
      app, GURL("chrome://settings")));
}

// Test that adding a web app without a manifest while using the
// |require_manifest| flag fails.
IN_PROC_BROWSER_TEST_F(PendingBookmarkAppManagerBrowserTest,
                       RequireManifestFailsIfNoManifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html"));
  InstallApp(url, false /* bypass_service_worker_check */,
             true /* require_manifest */);
  EXPECT_EQ(web_app::InstallResultCode::kFailedUnknownReason,
            result_code_.value());
  base::Optional<std::string> id =
      web_app::ExtensionIdsMap(browser()->profile()->GetPrefs())
          .LookupExtensionId(url);
  ASSERT_FALSE(id.has_value());
}

}  // namespace extensions
