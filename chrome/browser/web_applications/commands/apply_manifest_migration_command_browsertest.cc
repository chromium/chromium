// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/apply_manifest_migration_command.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/scheduler/apply_manifest_migration_result.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace web_app {
namespace {

class ApplyManifestMigrationCommandBrowserTest : public WebAppBrowserTestBase {
 public:
  static constexpr std::string_view kMigrateFromInstallUrl =
      "/web_apps/migration/migrate_from/no_migration_info.html";
  static constexpr std::string_view kMigrateFromManifestId =
      "/web_apps/migration/migrate_from/manifest_id";

  static constexpr std::string_view kMigrateToWithSuggestMigrationUrl =
      "/web_apps/migration/migrate_to/suggest.html";
  static constexpr std::string_view kMigrateToManifestId =
      "/web_apps/migration/migrate_to/manifest_id";

  ApplyManifestMigrationCommandBrowserTest() = default;
  ~ApplyManifestMigrationCommandBrowserTest() override = default;

  testing::AssertionResult WaitForAndApproveMigration(Browser* app_browser) {
    auto app_menu_model = std::make_unique<WebAppMenuModel>(
        /*provider=*/nullptr, app_browser);
    app_menu_model->Init();
    ui::MenuModel* model = app_menu_model.get();
    size_t index = 0;
    const bool found = app_menu_model->GetModelAndIndexForCommandId(
        IDC_WEB_APP_UPGRADE_DIALOG, &model, &index);
    if (!found) {
      return testing::AssertionFailure()
             << "No migration option found in app menu.";
    }
    if (!model->IsEnabledAt(index)) {
      return testing::AssertionFailure()
             << "Migration option is disabled in app menu.";
    }

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebAppUpdateReviewDialog");
    app_menu_model->ExecuteCommand(IDC_WEB_APP_UPGRADE_DIALOG,
                                   /*event_flags=*/0);
    views::Widget* active_update_dialog_widget = waiter.WaitIfNeededAndGet();
    app_menu_model.reset();

    views::test::AcceptDialog(active_update_dialog_widget);
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
    return testing::AssertionSuccess();
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kWebAppMigrationApi};

  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(ApplyManifestMigrationCommandBrowserTest,
                       MigrateFromSuggestedLaunchSuccess) {
  Browser* app_browser = InstallWebAppFromPageGetBrowser(
      browser(), https_server()->GetURL(kMigrateFromInstallUrl));

  // This should register the migration:
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server()->GetURL(kMigrateToWithSuggestMigrationUrl)));
  test::WaitForLoadCompleteAndMaybeManifestSeen(
      *browser()->tab_strip_model()->GetActiveWebContents());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // Approve the migration, waiting for the old browser to be closed and new one
  // to be created.
  ui_test_utils::BrowserDestroyedObserver browser_destroyed_observer(
      app_browser);
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  ASSERT_TRUE(WaitForAndApproveMigration(app_browser));
  browser_destroyed_observer.Wait();
  Browser* new_app_browser = browser_created_observer.Wait();

  EXPECT_TRUE(AppBrowserController::IsWebApp(new_app_browser));

  webapps::ManifestId migrate_to_manifest_id =
      webapps::ManifestId(https_server()->GetURL(kMigrateToManifestId));

  EXPECT_TRUE(provider().registrar_unsafe().AppMatches(
      GenerateAppIdFromManifestId(migrate_to_manifest_id),
      WebAppFilter::InstalledInOperatingSystemForTesting()));

  // Old app should be uninstalled.
  webapps::ManifestId migrate_from_manifest_id =
      webapps::ManifestId(https_server()->GetURL(kMigrateFromManifestId));
  EXPECT_FALSE(provider().registrar_unsafe().AppMatches(
      GenerateAppIdFromManifestId(migrate_from_manifest_id),
      WebAppFilter::InstalledInOperatingSystemForTesting()));

  histogram_tester_.ExpectUniqueSample(
      "WebApp.Migration.ApplyResult",
      /*sample=*/ApplyManifestMigrationResult::kAppMigrationAppliedSuccessfully,
      /*expected_bucket_count=*/1);
}

}  // namespace
}  // namespace web_app
