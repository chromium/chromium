// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/shortcut_handling_sub_manager.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"
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

class ShortcutHandlingSubManagerBrowserTest
    : public WebAppControllerBrowserTest,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  const int kTotalIconSizes = 9;

  void SetUpOnMainThread() override {
    os_hooks_suppress_.reset();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      shortcut_override_ =
          ShortcutOverrideForTesting::OverrideForTesting(base::GetHomeDir());
    }
    WebAppControllerBrowserTest::SetUpOnMainThread();
  }

  void SetUp() override {
    if (OsIntegrationSubManagersEnabled()) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers, {{"stage", "write_config"}});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kOsIntegrationSubManagers});
    }
    WebAppControllerBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      shortcut_override_.reset();
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

  void UninstallAppAndCleanData(const AppId& app_id) {
    base::test::TestFuture<webapps::UninstallResultCode> uninstall_future;
    provider().install_finalizer().UninstallWebApp(
        app_id, webapps::WebappUninstallSource::kAppsPage,
        uninstall_future.GetCallback());
    EXPECT_THAT(uninstall_future.Get(),
                testing::Eq(webapps::UninstallResultCode::kSuccess));
  }

  bool OsIntegrationSubManagersEnabled() {
    return GetParam() == OsIntegrationSubManagersState::kEnabled;
  }

 private:
  std::unique_ptr<ShortcutOverrideForTesting::BlockingRegistration>
      shortcut_override_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ShortcutHandlingSubManagerBrowserTest, Configure) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");

  const AppId& app_id = LoadUrlAndInstallApp(test_url);

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  if (OsIntegrationSubManagersEnabled()) {
    EXPECT_TRUE(state.has_value());
    EXPECT_THAT(state.value().shortcut_states().title(),
                testing::Eq("Manifest test app"));

    // All icons are read from the disk.
    EXPECT_THAT(state.value().shortcut_states().icon_data_any_size(),
                testing::Eq(kTotalIconSizes));

    for (const proto::ShortcutIconData& icon_time_map_data :
         state.value().shortcut_states().icon_data_any()) {
      EXPECT_THAT(
          syncer::ProtoTimeToTime(icon_time_map_data.timestamp()).is_null(),
          testing::IsFalse());
    }
  } else {
    EXPECT_FALSE(state.has_value());
  }

  // Do this to ensure when shortcut_override_ is reset, we do not have leftover
  // data.
  UninstallAppAndCleanData(app_id);
}

IN_PROC_BROWSER_TEST_P(ShortcutHandlingSubManagerBrowserTest,
                       ConfigureUninstallReturnsEmptyState) {
  GURL test_url = https_server()->GetURL(
      "/banners/"
      "manifest_test_page.html");
  const AppId& app_id = LoadUrlAndInstallApp(test_url);

  UninstallAppAndCleanData(app_id);
  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  EXPECT_FALSE(state.has_value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ShortcutHandlingSubManagerBrowserTest,
    ::testing::Values(OsIntegrationSubManagersState::kEnabled,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

}  // namespace

}  // namespace web_app
