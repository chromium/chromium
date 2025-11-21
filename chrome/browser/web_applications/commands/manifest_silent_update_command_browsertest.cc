// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace web_app {

namespace {

const base::FilePath kBlueIcon{
    FILE_PATH_LITERAL("chrome/test/data/web_apps/updating/blue-192.png")};
const base::FilePath kBlueRedIcon{
    FILE_PATH_LITERAL("chrome/test/data/web_apps/updating/blue-red-192.png")};
const base::FilePath kBlueWhiteIcon{
    FILE_PATH_LITERAL("chrome/test/data/web_apps/updating/blue-white-192.png")};

constexpr int kIconSizeToTest = 192;

constexpr const char kBypassSmallIconDiffThrottle[] =
    "bypass-small-icon-diff-throttle";

SkBitmap GetBitmapForInstalledAppOnDisk(const webapps::AppId& app_id,
                                        WebAppIconManager& icon_manager) {
  base::test::TestFuture<IconMetadataFromDisk> future;
  icon_manager.ReadTrustedIconsWithFallbackToManifestIcons(
      app_id, {kIconSizeToTest}, IconPurpose::ANY, future.GetCallback());
  web_app::SizeToBitmap icon_bitmaps = std::move(future.Take().icons_map);
  CHECK(!icon_bitmaps.empty());
  return icon_bitmaps[kIconSizeToTest];
}

}  // namespace

class ManifestSilentUpdateCommandBrowserTest : public WebAppBrowserTestBase {
 public:
  ManifestSilentUpdateCommandBrowserTest() {
    feature_list_.InitWithFeatures({features::kWebAppPredictableAppUpdating,
                                    features::kWebAppUsePrimaryIcon},
                                   {});
  }
  ~ManifestSilentUpdateCommandBrowserTest() override = default;

  void SetUpOnMainThread() override {
    clock_ = std::make_unique<base::SimpleTestClock>();
    provider().SetClockForTesting(clock_.get());
    WebAppBrowserTestBase::SetUpOnMainThread();
  }

  std::unique_ptr<base::SimpleTestClock> clock_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ManifestSilentUpdateCommandBrowserTest, SilentUpdate) {
  clock_->SetNow(base::Time::Now());
  const GURL app_url = https_server()->GetURL("/web_apps/updating/index.html");
  const webapps::AppId app_id = InstallWebAppFromPage(browser(), app_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  // TODO(crbug.com/442643377): Delete this wait after the update runs for every
  // navigation.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // Menu button should not have update available.
  WebAppMenuButton* const menu_button =
      static_cast<WebAppMenuButton*>(app_browser->GetBrowserView()
                                         .toolbar_button_provider()
                                         ->GetAppMenuButton());
  EXPECT_FALSE(menu_button->IsLabelPresentAndVisible());

  EXPECT_EQ(
      base::Time(),
      provider().registrar_unsafe().GetAppById(app_id)->manifest_update_time());

  EXPECT_EQ(app_url, provider().registrar_unsafe().GetAppStartUrl(app_id));

  const GURL update_url =
      https_server()->GetURL("/web_apps/updating/new_start_url_page.html");

  {
    UpdateAwaiter awaiter(provider().install_manager());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url));
    awaiter.AwaitUpdate();
    // Wait for the command to complete so all observers are notified.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->manifest_update_time(),
      clock_->Now());
  EXPECT_EQ(update_url, provider().registrar_unsafe().GetAppStartUrl(app_id));

  // This was silent, so menu button should not have update available still
  EXPECT_FALSE(menu_button->IsLabelPresentAndVisible());
}

IN_PROC_BROWSER_TEST_F(ManifestSilentUpdateCommandBrowserTest, PendingUpdate) {
  clock_->SetNow(base::Time::Now());
  const GURL app_url = https_server()->GetURL("/web_apps/updating/index.html");
  const webapps::AppId app_id = InstallWebAppFromPage(browser(), app_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  // TODO(crbug.com/442643377): Delete this wait after the update runs for every
  // navigation.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // Menu button should not have update available.
  WebAppMenuButton* const menu_button =
      static_cast<WebAppMenuButton*>(app_browser->GetBrowserView()
                                         .toolbar_button_provider()
                                         ->GetAppMenuButton());
  EXPECT_FALSE(menu_button->IsLabelPresentAndVisible());

  EXPECT_EQ(
      base::Time(),
      provider().registrar_unsafe().GetAppById(app_id)->manifest_update_time());

  EXPECT_EQ(app_url, provider().registrar_unsafe().GetAppStartUrl(app_id));

  const GURL update_url =
      https_server()->GetURL("/web_apps/updating/new_icon_page.html");

  {
    UpdateAwaiter awaiter(provider().install_manager());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url));
    awaiter.AwaitUpdate();
    // Wait for the command to complete so all observers are notified.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->manifest_update_time(),
      clock_->Now());
  EXPECT_EQ(update_url, provider().registrar_unsafe().GetAppStartUrl(app_id));

  // This has a new icon, so a pending update should be here.
  EXPECT_TRUE(menu_button->IsLabelPresentAndVisible());
  EXPECT_EQ(menu_button->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringFUTF16(
                IDS_WEB_APP_MENU_BUTTON_TOOLTIP_UPDATE_AVAILABLE,
                AppBrowserController::From(app_browser)->GetAppShortName()));
}

IN_PROC_BROWSER_TEST_F(ManifestSilentUpdateCommandBrowserTest,
                       ToolbarVisibilityUpdatedOnScopeChange) {
  const GURL app_url =
      https_server()->GetURL("/web_apps/scope_updating/page.html");
  const webapps::AppId app_id = InstallWebAppFromPage(browser(), app_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  // TODO(crbug.com/442643377): Delete this wait after the update runs for every
  // navigation.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  const GURL update_url =
      https_server()->GetURL("/web_apps/scope_updating/page_update.html");

  {
    UpdateAwaiter awaiter(
        WebAppProvider::GetForTest(browser()->profile())->install_manager());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url));
    awaiter.AwaitUpdate();
  }

  // After update, we are on the update page, which is in scope.
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());

  // Now navigate to the out-of-scope URL and check that the toolbar is
  // hidden because the scope has been widened.
  const GURL out_of_scope_url =
      https_server()->GetURL("/web_apps/scope_updating/out-of-scope.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, out_of_scope_url));
  EXPECT_FALSE(app_browser->app_controller()->ShouldShowCustomTabBar());
}

IN_PROC_BROWSER_TEST_F(ManifestSilentUpdateCommandBrowserTest,
                       SilentUpdateTenPercentIconDiffThrottled) {
  // First install the app.
  clock_->SetNow(base::Time::Now());
  const GURL app_url =
      https_server()->GetURL("/web_apps/updating/index_blue.html");
  const webapps::AppId app_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  // TODO(crbug.com/442643377): Delete this wait after the update runs for every
  // navigation.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(provider().registrar_unsafe().IsInRegistrar(app_id));
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      web_app::test::LoadTestImageFromDisk(kBlueIcon).AsBitmap(),
      GetBitmapForInstalledAppOnDisk(app_id, provider().icon_manager()),
      /*max_deviation=*/3));

  // Second, trigger an update to an app that has an icon of diff less than 10%.
  // Verify app gets updated silently.
  const GURL update_url =
      https_server()->GetURL("/web_apps/updating/index_blue_white.html");
  {
    UpdateAwaiter awaiter(provider().install_manager());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url));
    awaiter.AwaitUpdate();
    // Wait for the command to complete so all observers are notified.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->manifest_update_time(),
      clock_->Now());
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      web_app::test::LoadTestImageFromDisk(kBlueWhiteIcon).AsBitmap(),
      GetBitmapForInstalledAppOnDisk(app_id, provider().icon_manager()),
      /*max_deviation=*/3));

  // Forward time by 12 hours, trigger another silent update, this time of a
  // different icon that is still <10% diff away. Verify that the icon doesn't
  // get applied automatically, and that the pending icon info is stored in the
  // web app instead.
  clock_->Advance(base::Hours(12));
  const GURL update_url2 =
      https_server()->GetURL("/web_apps/updating/index_blue_red.html");
  const GURL pending_update_icon =
      https_server()->GetURL("/web_apps/updating/blue-red-192.png");
  {
    UpdateAwaiter awaiter(provider().install_manager());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url2));
    awaiter.AwaitUpdate();
    // Wait for the command to complete so all observers are notified.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }

  // The pending update info is silently "stored" on the web app, and the
  // start_url is updated, so the manifest update is "still" counted.
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->manifest_update_time(),
      clock_->Now());

  // The app icons on the disk have not been updated, so assert that the old
  // icons still remain.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      web_app::test::LoadTestImageFromDisk(kBlueWhiteIcon).AsBitmap(),
      GetBitmapForInstalledAppOnDisk(app_id, provider().icon_manager()),
      /*max_deviation=*/3));

  // The silent icons should be stored in the web_app as a pending update.
  const WebApp* web_app = provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_NE(nullptr, web_app);
  ASSERT_TRUE(web_app->pending_update_info().has_value());
  EXPECT_EQ(1, web_app->pending_update_info()->trusted_icons().size());
  EXPECT_EQ(pending_update_icon,
            web_app->pending_update_info()->trusted_icons().begin()->url());
}

IN_PROC_BROWSER_TEST_F(ManifestSilentUpdateCommandBrowserTest,
                       BypassIconThrottleAndUpdateAfter24Hours) {
  // First install the app.
  clock_->SetNow(base::Time::Now());
  const GURL app_url =
      https_server()->GetURL("/web_apps/updating/index_blue.html");
  const webapps::AppId app_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  // TODO(crbug.com/442643377): Delete this wait after the update runs for every
  // navigation.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(provider().registrar_unsafe().IsInRegistrar(app_id));
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      web_app::test::LoadTestImageFromDisk(kBlueIcon).AsBitmap(),
      GetBitmapForInstalledAppOnDisk(app_id, provider().icon_manager()),
      /*max_deviation=*/3));

  // Second, trigger an update to an app that has an icon of diff less than 10%.
  // Verify app gets updated silently.
  const GURL update_url =
      https_server()->GetURL("/web_apps/updating/index_blue_white.html");
  {
    UpdateAwaiter awaiter(provider().install_manager());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url));
    awaiter.AwaitUpdate();
    // Wait for the command to complete so all observers are notified.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->manifest_update_time(),
      clock_->Now());
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      web_app::test::LoadTestImageFromDisk(kBlueWhiteIcon).AsBitmap(),
      GetBitmapForInstalledAppOnDisk(app_id, provider().icon_manager()),
      /*max_deviation=*/3));

  // Forward time by more than 24 hours, trigger another silent update, this
  // time of a different icon that is still <10% diff away. Verify that the
  // update succeeds silently, and a pending update info is not stored.
  clock_->Advance(base::Hours(28));
  const GURL update_url2 =
      https_server()->GetURL("/web_apps/updating/index_blue_red.html");
  {
    UpdateAwaiter awaiter(provider().install_manager());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url2));
    awaiter.AwaitUpdate();
    // Wait for the command to complete so all observers are notified.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->manifest_update_time(),
      clock_->Now());

  // The app icons on the disk should be updated now.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      web_app::test::LoadTestImageFromDisk(kBlueRedIcon).AsBitmap(),
      GetBitmapForInstalledAppOnDisk(app_id, provider().icon_manager()),
      /*max_deviation=*/3));

  // The silent icons should not be stored in the web_app as a pending update.
  const WebApp* web_app = provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_NE(nullptr, web_app);
  ASSERT_FALSE(web_app->pending_update_info().has_value());
}

IN_PROC_BROWSER_TEST_F(ManifestSilentUpdateCommandBrowserTest,
                       MenuButtonClearedDynamically) {
  // First, install a web app.
  clock_->SetNow(base::Time::Now());
  const GURL app_url = https_server()->GetURL("/web_apps/updating/index.html");
  const webapps::AppId app_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  // TODO(crbug.com/442643377): Delete this wait after the update runs for every
  // navigation.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // Menu button should not have update available.
  WebAppMenuButton* const menu_button =
      static_cast<WebAppMenuButton*>(app_browser->GetBrowserView()
                                         .toolbar_button_provider()
                                         ->GetAppMenuButton());
  EXPECT_FALSE(menu_button->IsLabelPresentAndVisible());
  EXPECT_EQ(app_url, provider().registrar_unsafe().GetAppStartUrl(app_id));

  // Second, trigger a security sensitive update, verify pending update stored
  // in the app, and menu button has the "App Update Available" expanded state.
  const GURL update_url =
      https_server()->GetURL("/web_apps/updating/new_icon_page.html");
  {
    base::test::TestFuture<void> update_future;
    UpdateAwaiter awaiter(provider().install_manager());
    auto subscription = menu_button->AwaitLabelTextUpdated(
        update_future.GetRepeatingCallback());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url));
    awaiter.AwaitUpdate();
    // Wait for the command to complete so all observers are notified.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
    EXPECT_TRUE(update_future.Wait());
  }
  EXPECT_EQ(update_url, provider().registrar_unsafe().GetAppStartUrl(app_id));

  // This has a new icon, so a pending update should be there in the app.
  EXPECT_TRUE(provider()
                  .registrar_unsafe()
                  .GetAppById(app_id)
                  ->pending_update_info()
                  .has_value());
  EXPECT_TRUE(menu_button->IsLabelPresentAndVisible());

  // Third, trigger a non-security sensitive update, and revert the name changes
  // back. Verify that the menu button no longer has the "App Update Available"
  // expanded state.
  const GURL update_url2 =
      https_server()->GetURL("/web_apps/updating/new_start_url_page.html");
  {
    base::test::TestFuture<void> update_future;
    UpdateAwaiter awaiter(provider().install_manager());
    auto subscription = menu_button->AwaitLabelTextUpdated(
        update_future.GetRepeatingCallback());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url2));
    awaiter.AwaitUpdate();
    // Wait for the command to complete so all observers are notified.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
    EXPECT_TRUE(update_future.Wait());
  }
  EXPECT_EQ(update_url2, provider().registrar_unsafe().GetAppStartUrl(app_id));
  // The pending update info should be removed from the app.
  EXPECT_FALSE(provider()
                   .registrar_unsafe()
                   .GetAppById(app_id)
                   ->pending_update_info()
                   .has_value());
  EXPECT_FALSE(menu_button->IsLabelPresentAndVisible());
}

// Used to verify that if the `kBypassSmallIconDiffThrottle` flag is used,
// the throttle for limiting silent icon updates of small diffs to once per day
// can be bypassed.
class ManifestSilentUpdateCommandLineTests
    : public ManifestSilentUpdateCommandBrowserTest {
 public:
  ManifestSilentUpdateCommandLineTests() = default;
  ~ManifestSilentUpdateCommandLineTests() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(kBypassSmallIconDiffThrottle);
  }
};

IN_PROC_BROWSER_TEST_F(ManifestSilentUpdateCommandLineTests,
                       ThrottleBypassViaCmdLine) {
  // First install the app.
  clock_->SetNow(base::Time::Now());
  const GURL app_url =
      https_server()->GetURL("/web_apps/updating/index_blue.html");
  const webapps::AppId app_id =
      InstallWebAppFromPageAndCloseAppBrowser(browser(), app_url);
  Browser* app_browser = LaunchWebAppBrowser(app_id);
  // TODO(crbug.com/442643377): Delete this wait after the update runs for every
  // navigation.
  provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_TRUE(provider().registrar_unsafe().IsInRegistrar(app_id));
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      web_app::test::LoadTestImageFromDisk(kBlueIcon).AsBitmap(),
      GetBitmapForInstalledAppOnDisk(app_id, provider().icon_manager()),
      /*max_deviation=*/3));

  // Second, trigger an update to an app that has an icon of diff less than 10%.
  // Verify app gets updated silently.
  const GURL update_url =
      https_server()->GetURL("/web_apps/updating/index_blue_white.html");
  {
    UpdateAwaiter awaiter(provider().install_manager());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url));
    awaiter.AwaitUpdate();
    // Wait for the command to complete so all observers are notified.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->manifest_update_time(),
      clock_->Now());
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      web_app::test::LoadTestImageFromDisk(kBlueWhiteIcon).AsBitmap(),
      GetBitmapForInstalledAppOnDisk(app_id, provider().icon_manager()),
      /*max_deviation=*/3));

  // Forward time by less than 24 hours, trigger another silent update, this
  // time of a different icon that is still <10% diff away. Verify that the
  // update succeeds silently, and a pending update info is not stored.
  clock_->Advance(base::Hours(12));
  const GURL update_url2 =
      https_server()->GetURL("/web_apps/updating/index_blue_red.html");
  {
    UpdateAwaiter awaiter(provider().install_manager());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, update_url2));
    awaiter.AwaitUpdate();
    // Wait for the command to complete so all observers are notified.
    provider().command_manager().AwaitAllCommandsCompleteForTesting();
  }
  EXPECT_EQ(
      provider().registrar_unsafe().GetAppById(app_id)->manifest_update_time(),
      clock_->Now());

  // The app icons on the disk should be updated now.
  EXPECT_TRUE(gfx::test::AreBitmapsClose(
      web_app::test::LoadTestImageFromDisk(kBlueRedIcon).AsBitmap(),
      GetBitmapForInstalledAppOnDisk(app_id, provider().icon_manager()),
      /*max_deviation=*/3));

  // The silent icons should not be stored in the web_app as a pending update.
  const WebApp* web_app = provider().registrar_unsafe().GetAppById(app_id);
  ASSERT_NE(nullptr, web_app);
  ASSERT_FALSE(web_app->pending_update_info().has_value());
}

}  // namespace web_app
