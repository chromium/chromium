// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_mover.h"

#include "base/bind.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {
class WebAppMoverBrowsertestBase : public InProcessBrowserTest {
 public:
  WebAppMoverBrowsertestBase() {
    suppress_hooks_ = OsIntegrationManager::ScopedSuppressOsHooksForTesting();
    embedded_test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    // Since the port is a part of the start_url, this needs to stay consistent
    // between the tests below.
    CHECK(embedded_test_server()->Start(16247));
    switch (GetTestPreCount()) {
      case 1:
        WebAppMover::DisableForTesting();
        break;
      case 0:
        WebAppMover::SetCompletedCallbackForTesting(
            base::BindLambdaForTesting([this]() {
              clean_up_completed_ = true;
              if (completed_callback_)
                std::move(completed_callback_).Run();
            }));
        break;
    }
  }
  ~WebAppMoverBrowsertestBase() override = default;

 protected:
  GURL GetMigratingFromAppA() {
    return embedded_test_server()->GetURL(
        "/web_apps/mover/migrate_from/a/index.html");
  }

  GURL GetMigratingFromAppB() {
    return embedded_test_server()->GetURL(
        "/web_apps/mover/migrate_from/b/index.html");
  }

  GURL GetMigratingFromAppC() {
    return embedded_test_server()->GetURL(
        "/web_apps/mover/migrate_from/c/index.html");
  }

  GURL GetMigratingToApp() {
    return embedded_test_server()->GetURL(
        "/web_apps/mover/migrate_to/index.html");
  }

  AppId InstallApp(GURL url) {
    ui_test_utils::NavigateToURL(browser(), url);

    AppId app_id;
    base::RunLoop run_loop;
    GetProvider().install_manager().InstallWebAppFromManifestWithFallback(
        browser()->tab_strip_model()->GetActiveWebContents(),
        /*force_shortcut_app=*/false,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        base::BindOnce(TestAcceptDialogCallback),
        base::BindLambdaForTesting(
            [&](const AppId& new_app_id, InstallResultCode code) {
              EXPECT_EQ(code, InstallResultCode::kSuccessNewInstall) << code;
              app_id = new_app_id;
              run_loop.Quit();
            }));
    run_loop.Run();
    return app_id;
  }

  WebAppProviderBase& GetProvider() {
    return *WebAppProviderBase::GetProviderBase(browser()->profile());
  }

  bool clean_up_completed_ = false;
  base::OnceClosure completed_callback_;

 private:
  ScopedOsHooksSuppress suppress_hooks_;
};

class WebAppMoverPrefixBrowsertest : public WebAppMoverBrowsertestBase {
 public:
  WebAppMoverPrefixBrowsertest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kMoveWebApp,
          {{features::kMoveWebAppUninstallStartUrlPrefix.name,
            embedded_test_server()
                ->GetURL("/web_apps/mover/migrate_from/")
                .spec()},
           {features::kMoveWebAppInstallStartUrl.name,
            GetMigratingToApp().spec()}}}},
        {});
  }
  ~WebAppMoverPrefixBrowsertest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppMoverPrefixBrowsertest, PRE_TestMigration) {
  InstallApp(GetMigratingFromAppA());
  InstallApp(GetMigratingFromAppB());
}

IN_PROC_BROWSER_TEST_F(WebAppMoverPrefixBrowsertest, TestMigration) {
  // Wait for clean up to complete (this will timeout if we don't run clean up).
  if (!clean_up_completed_) {
    base::RunLoop run_loop;
    completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  ASSERT_EQ(GetProvider().registrar().GetAppIds().size(), 1ul);
  EXPECT_EQ(GetProvider().registrar().GetAppIds().front(),
            GenerateAppIdFromURL(GetMigratingToApp()));
}

class WebAppMoverPatternBrowsertest : public WebAppMoverBrowsertestBase {
 public:
  WebAppMoverPatternBrowsertest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kMoveWebApp,
          {{features::kMoveWebAppUninstallStartUrlPattern.name,
            embedded_test_server()
                ->GetURL("/web_apps/mover/migrate_from/[ac]/.*")
                .spec()},
           {features::kMoveWebAppInstallStartUrl.name,
            GetMigratingToApp().spec()}}}},
        {});
  }
  ~WebAppMoverPatternBrowsertest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppMoverPatternBrowsertest, PRE_TestMigration) {
  InstallApp(GetMigratingFromAppA());
  InstallApp(GetMigratingFromAppB());
  InstallApp(GetMigratingFromAppC());
}

IN_PROC_BROWSER_TEST_F(WebAppMoverPatternBrowsertest, TestMigration) {
  // Wait for clean up to complete (this will timeout if we don't run clean up).
  if (!clean_up_completed_) {
    base::RunLoop run_loop;
    completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  EXPECT_THAT(GetProvider().registrar().GetAppIds(),
              testing::UnorderedElementsAre(
                  GenerateAppIdFromURL(GetMigratingToApp()),
                  GenerateAppIdFromURL(GetMigratingFromAppB())));
}

// The WebAppMover requires a full match. This tests that a partial match
// doesn't trigger migration. If a '.*' is added at the end of the pattern, then
// the migration would happen.
class WebAppMoverBadPatternBrowsertest : public WebAppMoverBrowsertestBase {
 public:
  WebAppMoverBadPatternBrowsertest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kMoveWebApp,
          {{features::kMoveWebAppUninstallStartUrlPattern.name,
            embedded_test_server()
                ->GetURL("/web_apps/mover/migrate_from/[ac]")
                .spec()},
           {features::kMoveWebAppInstallStartUrl.name,
            GetMigratingToApp().spec()}}}},
        {});
  }
  ~WebAppMoverBadPatternBrowsertest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppMoverBadPatternBrowsertest, PRE_TestMigration) {
  InstallApp(GetMigratingFromAppA());
  InstallApp(GetMigratingFromAppB());
  InstallApp(GetMigratingFromAppC());
  EXPECT_EQ(GetProvider().registrar().GetAppIds().size(), 3ul);
}

IN_PROC_BROWSER_TEST_F(WebAppMoverBadPatternBrowsertest, TestMigration) {
  // Wait for clean up to complete (this will timeout if we don't run clean up).
  if (!clean_up_completed_) {
    base::RunLoop run_loop;
    completed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  EXPECT_EQ(GetProvider().registrar().GetAppIds().size(), 3ul);

  EXPECT_THAT(GetProvider().registrar().GetAppIds(),
              testing::UnorderedElementsAre(
                  GenerateAppIdFromURL(GetMigratingFromAppA()),
                  GenerateAppIdFromURL(GetMigratingFromAppB()),
                  GenerateAppIdFromURL(GetMigratingFromAppC())));
}

}  // namespace
}  // namespace web_app
