// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_expected_support.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/app_menu_button_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/test/widget_test.h"

namespace web_app {
namespace {

enum class AppType { kRegular, kTabbed, kIsolated };

AppType AppTypeFromTestName() {
  const std::string name = base::TestNameWithoutDisabledPrefix(
      testing::UnitTest::GetInstance()->current_test_info()->name());
  size_t underscore = name.rfind('_');
  std::string suffix = underscore == std::string::npos
                           ? std::string()
                           : name.substr(underscore + 1);
  if (suffix == "tabbed") {
    return AppType::kTabbed;
  } else if (suffix == "isolated") {
    return AppType::kIsolated;
  }
  return AppType::kRegular;
}

}  // namespace

class WebAppMenuBrowserTest
    : public SupportsTestUi<IsolatedWebAppBrowserTestHarness, TestBrowserUi> {
 public:
  WebAppMenuBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kWebAppPredictableAppUpdating,
         blink::features::kDesktopPWAsTabStrip,
         blink::features::kDesktopPWAsTabStripCustomizations},
        {});
  }

  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    const AppType app_type = AppTypeFromTestName();
    if (app_type == AppType::kIsolated) {
      isolated_web_app_ =
          IsolatedWebAppBuilder(ManifestBuilder()).BuildBundle();
      ASSERT_OK_AND_ASSIGN(IsolatedWebAppUrlInfo url_info,
                           isolated_web_app_->Install(profile()));
      app_id_ = url_info.app_id();
    } else {
      auto web_app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
          GURL("https://test.org"));
      web_app_info->title = u"Test App";
      web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
      if (app_type == AppType::kTabbed) {
        web_app_info->display_override = {blink::mojom::DisplayMode::kTabbed};
        web_app_info->title = u"Tabbed App";
      }
      app_id_ = InstallWebApp(std::move(web_app_info));
    }

    app_browser_ = LaunchWebAppBrowser(app_id_);

    // Wait for the app browser to be visible. Without this the menu will close
    // without even being shown.
    views::test::WidgetVisibleWaiter waiter(
        BrowserView::GetBrowserViewForBrowser(app_browser_)->GetWidget());
    waiter.Wait();
  }

  void TearDownOnMainThread() override {
    app_browser_ = nullptr;
    UninstallWebApp(app_id_);
    IsolatedWebAppBrowserTestHarness::TearDownOnMainThread();
  }

  // UiBrowserTest:
  void ShowUi(const std::string& name) override;
  bool VerifyUi() override;
  void WaitForUserDismissal() override;

 protected:
  WebAppMenuButton* menu_button() {
    return static_cast<WebAppMenuButton*>(
        BrowserView::GetBrowserViewForBrowser(app_browser_)
            ->toolbar_button_provider()
            ->GetAppMenuButton());
  }

  const webapps::AppId& app_id() const { return app_id_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  webapps::AppId app_id_;
  std::unique_ptr<ScopedBundledIsolatedWebApp> isolated_web_app_;

  raw_ptr<Browser> app_browser_ = nullptr;
};

void WebAppMenuBrowserTest::ShowUi(const std::string& name) {
  // Include mnemonics in screenshots so that we detect changes to them.
  menu_button()->ShowMenu(views::MenuRunner::SHOULD_SHOW_MNEMONICS);
}

bool WebAppMenuBrowserTest::VerifyUi() {
  if (!menu_button()->IsMenuShowing()) {
    return false;
  }
  views::MenuItemView* menu_item = menu_button()->app_menu()->root_menu_item();

  const auto* const test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  return VerifyPixelUi(menu_item->GetSubmenu()->GetScrollViewContainer(),
                       test_info->test_suite_name(),
                       test_info->name()) != ui::test::ActionResult::kFailed;
}

void WebAppMenuBrowserTest::WaitForUserDismissal() {
  base::RunLoop run_loop;
  class CloseWaiter : public AppMenuButtonObserver {
   public:
    explicit CloseWaiter(base::RepeatingClosure quit_closure)
        : quit_closure_(std::move(quit_closure)) {}

    // AppMenuButtonObserver:
    void AppMenuClosed() override { quit_closure_.Run(); }

   private:
    const base::RepeatingClosure quit_closure_;
  } waiter(run_loop.QuitClosure());

  base::ScopedObservation<AppMenuButton, CloseWaiter> observation(&waiter);
  observation.Observe(menu_button());

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppMenuBrowserTest, InvokeUi_main) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppMenuBrowserTest, InvokeUi_main_pending_update) {
  {
    ScopedRegistryUpdate update = provider().sync_bridge_unsafe().BeginUpdate();
    proto::PendingUpdateInfo update_info;
    update_info.set_name("Updated app name");
    update_info.set_was_ignored(false);
    update->UpdateApp(app_id())->SetPendingUpdateInfo(std::move(update_info));
  }

  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppMenuBrowserTest, InvokeUi_main_tabbed) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppMenuBrowserTest, InvokeUi_main_isolated) {
  ShowAndVerifyUi();
}

}  // namespace web_app
