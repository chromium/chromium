// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/test/web_app_page_waiter.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

using apps::Condition;
using apps::ConditionType;
using apps::IntentFilterPtr;
using apps::PatternMatchType;

namespace web_app {

namespace {

void CheckShareTextFilter(const IntentFilterPtr& intent_filter) {
  EXPECT_FALSE(intent_filter->activity_name.has_value());
  EXPECT_FALSE(intent_filter->activity_label.has_value());

  ASSERT_EQ(intent_filter->conditions.size(), 2U);

  {
    const Condition& condition = *intent_filter->conditions[0];
    EXPECT_EQ(condition.condition_type, ConditionType::kAction);
    ASSERT_EQ(condition.condition_values.size(), 1U);

    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kLiteral);
    EXPECT_EQ(condition.condition_values[0]->value, "send");
  }

  const Condition& condition = *intent_filter->conditions[1];
  EXPECT_EQ(condition.condition_type, ConditionType::kMimeType);
  ASSERT_EQ(condition.condition_values.size(), 1U);

  EXPECT_EQ(condition.condition_values[0]->match_type,
            PatternMatchType::kMimeType);
  EXPECT_EQ(condition.condition_values[0]->value, "text/plain");

  EXPECT_TRUE(
      apps_util::MakeShareIntent("text", "title")->MatchFilter(intent_filter));

  std::vector<GURL> filesystem_urls(1U);
  std::vector<std::string> mime_types(1U, "audio/mp3");
  EXPECT_FALSE(apps_util::MakeShareIntent(filesystem_urls, mime_types)
                   ->MatchFilter(intent_filter));
}

void CheckShareFileFilter(const IntentFilterPtr& intent_filter,
                          const std::vector<std::string>& filter_types,
                          const std::vector<std::string>& accepted_types,
                          const std::vector<std::string>& rejected_types) {
  EXPECT_FALSE(intent_filter->activity_name.has_value());
  EXPECT_FALSE(intent_filter->activity_label.has_value());

  ASSERT_EQ(intent_filter->conditions.size(), filter_types.empty() ? 1U : 2U);

  {
    const Condition& condition = *intent_filter->conditions[0];
    EXPECT_EQ(condition.condition_type, ConditionType::kAction);
    ASSERT_EQ(condition.condition_values.size(), 2U);

    EXPECT_EQ(condition.condition_values[0]->match_type,
              PatternMatchType::kLiteral);
    EXPECT_EQ(condition.condition_values[0]->value, "send");

    EXPECT_EQ(condition.condition_values[1]->match_type,
              PatternMatchType::kLiteral);
    EXPECT_EQ(condition.condition_values[1]->value, "send_multiple");
  }

  if (!filter_types.empty()) {
    const Condition& condition = *intent_filter->conditions[1];
    EXPECT_EQ(condition.condition_type, ConditionType::kFile);
    ASSERT_EQ(condition.condition_values.size(), filter_types.size());

    for (unsigned i = 0; i < filter_types.size(); ++i) {
      EXPECT_EQ(condition.condition_values[i]->match_type,
                PatternMatchType::kMimeType);
      EXPECT_EQ(condition.condition_values[i]->value, filter_types[i]);
    }
  }

  for (const std::string& accepted_type : accepted_types) {
    {
      std::vector<GURL> filesystem_urls(1U);
      std::vector<std::string> mime_types(1U, accepted_type);
      EXPECT_TRUE(apps_util::MakeShareIntent(filesystem_urls, mime_types)
                      ->MatchFilter(intent_filter));
    }

    {
      std::vector<GURL> filesystem_urls(3U);
      std::vector<std::string> mime_types(3U, accepted_type);
      EXPECT_TRUE(apps_util::MakeShareIntent(filesystem_urls, mime_types)
                      ->MatchFilter(intent_filter));
    }
  }

  for (const std::string& rejected_type : rejected_types) {
    std::vector<GURL> filesystem_urls(1U);
    std::vector<std::string> mime_types(1U, rejected_type);
    EXPECT_FALSE(apps_util::MakeShareIntent(filesystem_urls, mime_types)
                     ->MatchFilter(intent_filter));
  }
}

}  // namespace

using WebAppPublisherHelperBrowserTest = WebAppBrowserTestBase;

IN_PROC_BROWSER_TEST_F(WebAppPublisherHelperBrowserTest, CreateIntentFilters) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_share_target/charts.html"));

  apps::IntentFilters filters;
  {
    auto& provider = *web_app::WebAppProvider::GetForTest(browser()->profile());
    const webapps::AppId app_id =
        web_app::InstallWebAppFromManifest(browser(), app_url);
    filters = WebAppPublisherHelper::CreateIntentFiltersForWebApp(
        provider, *provider.registrar_unsafe().GetAppById(app_id));
  }

  ASSERT_EQ(filters.size(), 3U);

  EXPECT_TRUE(std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                             app_url.GetWithoutFilename())
                  ->MatchFilter(filters[0]));

  CheckShareTextFilter(filters[1]);

  const std::vector<std::string> filter_types(
      {"text/*", "image/svg+xml", "*/*"});
  const std::vector<std::string> accepted_types(
      {"text/plain", "image/svg+xml", "video/webm"});
  const std::vector<std::string> rejected_types;  // No types are rejected.
  CheckShareFileFilter(filters[2], filter_types, accepted_types,
                       rejected_types);
}

IN_PROC_BROWSER_TEST_F(WebAppPublisherHelperBrowserTest, PartialWild) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_share_target/partial-wild.html"));

  apps::IntentFilters filters;
  {
    auto& provider = *web_app::WebAppProvider::GetForTest(browser()->profile());
    const webapps::AppId app_id =
        web_app::InstallWebAppFromManifest(browser(), app_url);
    filters = WebAppPublisherHelper::CreateIntentFiltersForWebApp(
        provider, *provider.registrar_unsafe().GetAppById(app_id));
  }

  ASSERT_EQ(filters.size(), 2U);

  EXPECT_TRUE(std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                             app_url.GetWithoutFilename())
                  ->MatchFilter(filters[0]));

  const std::vector<std::string> filter_types({"image/*"});
  const std::vector<std::string> accepted_types({"image/png", "image/svg+xml"});
  const std::vector<std::string> rejected_types(
      {"application/vnd.android.package-archive", "text/plain"});
  CheckShareFileFilter(filters[1], filter_types, accepted_types,
                       rejected_types);
}

IN_PROC_BROWSER_TEST_F(WebAppPublisherHelperBrowserTest,
                       ShareTargetWithoutFiles) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(
      embedded_test_server()->GetURL("/web_share_target/poster.html"));

  apps::IntentFilters filters;
  {
    auto& provider = *web_app::WebAppProvider::GetForTest(browser()->profile());
    const webapps::AppId app_id =
        web_app::InstallWebAppFromManifest(browser(), app_url);
    filters = WebAppPublisherHelper::CreateIntentFiltersForWebApp(
        provider, *provider.registrar_unsafe().GetAppById(app_id));
  }

  ASSERT_EQ(filters.size(), 2U);

  EXPECT_TRUE(std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                             app_url.GetWithoutFilename())
                  ->MatchFilter(filters[0]));

  CheckShareTextFilter(filters[1]);
}

class WebAppPublisherHelperMigrationTest
    : public WebAppPublisherHelperBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      blink::features::kWebAppMigrationApi};
};

IN_PROC_BROWSER_TEST_F(WebAppPublisherHelperMigrationTest, MigrationCalls) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1. Install source app.
  const GURL from_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_from/no_migration_info.html");
  Browser* app_browser =
      web_app::InstallWebAppFromPageGetBrowser(browser(), from_url);
  const webapps::AppId source_app_id =
      web_app::AppBrowserController::From(app_browser)->app_id();

  auto* proxy =
      apps::AppServiceProxyFactory::GetForProfile(browser()->profile());

  // Verify source app is installed.
  bool source_found = proxy->AppRegistryCache().ForOneApp(
      source_app_id, [](const apps::AppUpdate& update) {
        EXPECT_EQ(update.Readiness(), apps::Readiness::kReady);
      });
  EXPECT_TRUE(source_found);

  // 2. Navigate to destination app to trigger migration.
  GURL to_url = embedded_test_server()->GetURL(
      "/web_apps/migration/migrate_to/suggest.html");
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), to_url));
  EXPECT_TRUE(web_app::test::WebAppPageWaiter(
                  browser()->tab_strip_model()->GetActiveWebContents())
                  .ExpectUrl(to_url)
                  .ManifestOrLoadedNoManifest()
                  .WaitAndFlushCommands());

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForTest(browser()->profile());

  // 3. Accept migration dialog.
  views::NamedWidgetShownWaiter update_dialog_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppUpdateReviewDialog");
  {
    chrome::Reload(app_browser, WindowOpenDisposition::CURRENT_TAB);
    WebAppMenuModel model(/*provider=*/nullptr, app_browser);
    model.Init();
    model.ExecuteCommand(IDC_WEB_APP_UPGRADE_DIALOG, /*event_flags=*/0);
  }
  views::Widget* dialog_widget = update_dialog_waiter.WaitIfNeededAndGet();
  ASSERT_NE(nullptr, dialog_widget);
  views::test::AcceptDialog(dialog_widget);
  provider->command_manager().AwaitAllCommandsCompleteForTesting();

  // 4. Verify AppService state.
  proxy->AppRegistryCache().ForOneApp(
      source_app_id, [](const apps::AppUpdate& update) {
        EXPECT_EQ(update.Readiness(), apps::Readiness::kUninstalledByUser);
      });

  // Verify target app is installed using its generated App ID, with the
  // `manifest_id` obtained from
  // `web_apps/migration/migrate_to/manifest_suggest.json`.
  const webapps::ManifestId target_manifest_id = web_app::GenerateManifestId(
      "web_apps/migration/migrate_to/manifest_id", to_url);
  const webapps::AppId target_app_id =
      web_app::GenerateAppIdFromManifestId(target_manifest_id);
  bool target_found = proxy->AppRegistryCache().ForOneApp(
      target_app_id, [](const apps::AppUpdate& update) {
        EXPECT_EQ(update.Readiness(), apps::Readiness::kReady);
      });
  EXPECT_TRUE(target_found);
}

}  // namespace web_app
