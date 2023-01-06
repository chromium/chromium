// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/shortcut_sub_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/base/time.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

using ::testing::Eq;
using ::testing::IsFalse;

namespace {

class ShortcutSubManagerBrowserTest
    : public WebAppControllerBrowserTest,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  const int kTotalIconSizes = 9;

  void SetUpOnMainThread() override {
    os_hooks_suppress_.reset();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ =
          OsIntegrationTestOverride::OverrideForTesting(base::GetHomeDir());
    }
    WebAppControllerBrowserTest::SetUpOnMainThread();
  }

  void SetUp() override {
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateToDB) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers, {{"stage", "write_config"}});
    } else if (GetParam() ==
               OsIntegrationSubManagersState::kSaveStateAndExecute) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers,
          {{"stage", "execute_and_write_config"}});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kOsIntegrationSubManagers});
    }
    WebAppControllerBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
    test::UninstallAllWebApps(profile());
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_.reset();
    }
    WebAppControllerBrowserTest::TearDownOnMainThread();
  }

  AppId LoadUrlAndInstallApp(const GURL& url) {
    EXPECT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), url));
    base::test::TestFuture<const AppId, webapps::InstallResultCode> test_future;
    provider().scheduler().FetchManifestAndInstall(
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
        /*bypass_service_worker_check=*/false,
        base::BindOnce(test::TestAcceptDialogCallback),
        test_future.GetCallback<const AppId&, webapps::InstallResultCode>(),
        /*use_fallback=*/false);
    EXPECT_THAT(test_future.Get<webapps::InstallResultCode>(),
                testing::Eq(webapps::InstallResultCode::kSuccessNewInstall));
    return test_future.Get<AppId>();
  }

  void UninstallWebApp(const AppId& app_id) {
    base::test::TestFuture<webapps::UninstallResultCode> uninstall_future;
    provider().install_finalizer().UninstallWebApp(
        app_id, webapps::WebappUninstallSource::kAppsPage,
        uninstall_future.GetCallback());
    EXPECT_THAT(uninstall_future.Get(),
                testing::Eq(webapps::UninstallResultCode::kSuccess));
  }

 private:
  std::unique_ptr<OsIntegrationTestOverride::BlockingRegistration>
      test_override_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ShortcutSubManagerBrowserTest, Configure) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");

  const AppId& app_id = LoadUrlAndInstallApp(test_url);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  if (AreOsIntegrationSubManagersEnabled()) {
    ASSERT_THAT(state.value().shortcut().title(),
                testing::Eq("Manifest test app"));
    // All icons are read from the disk.
    ASSERT_THAT(state.value().shortcut().icon_data_any_size(),
                testing::Eq(kTotalIconSizes));

    for (const proto::ShortcutIconData& icon_time_map_data :
         state.value().shortcut().icon_data_any()) {
      ASSERT_THAT(
          syncer::ProtoTimeToTime(icon_time_map_data.timestamp()).is_null(),
          testing::IsFalse());
    }
    // TODO(dmurph): Implement shortcut & color detection if
    // `AreSubManagersExecuteEnabled()` returns true. https://crbug.com/1404032.
  } else {
    ASSERT_FALSE(state.value().has_shortcut());
  }
}

IN_PROC_BROWSER_TEST_P(ShortcutSubManagerBrowserTest,
                       ConfigureUninstallReturnsEmptyState) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  const AppId& app_id = LoadUrlAndInstallApp(test_url);

  test::UninstallAllWebApps(profile());
  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  EXPECT_FALSE(state.has_value());

  // TODO(dmurph): Implement shortcut & color detection if
  // `AreSubManagersExecuteEnabled()` returns true. https://crbug.com/1404032.
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ShortcutSubManagerBrowserTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateToDB,
                      OsIntegrationSubManagersState::kSaveStateAndExecute,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

}  // namespace

}  // namespace web_app
