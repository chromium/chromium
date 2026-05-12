// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/web_apps/progress_delay.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_options_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_progress_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_link_capturing_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace web_app {

namespace {

class WebAppInstallFlowBrowserTest : public WebAppBrowserTestBase {
 public:
  WebAppInstallFlowBrowserTest()
      : progress_delay_override_(ProgressDelay::SetDurationOverrideForTesting(
            base::Milliseconds(0))) {
  }

  ~WebAppInstallFlowBrowserTest() override = default;

  void AdvanceToDoneAndAccept(views::Widget* widget) {
    ASSERT_TRUE(widget);
    // Step 1: Install Dialog. Accept to move to options.
    AcceptWidgetAndMoveForward(widget);

#if !BUILDFLAG(IS_LINUX)
    // Step 2: Installer Options, only shows up on non Linux install flows.
    // Accept to move to progress.
    AcceptWidgetAndMoveForward(widget);
#endif  //! BUILDFLAG(IS_LINUX)

    ForwardThroughProgressViewAndAcceptDialog(widget);
  }

  // Waiter for the new install flow dialog. Waits for the progress bar to be
  // complete, and then moves the installation forward until it reaches the
  // "Success" view. Successfully accepts that dialog, which should result in an
  // app being installed.
  // The input here should be the "WebAppInstallerOptionsView", this will
  // CHECK-fail if any other view is passed as an input here.
  void ForwardThroughProgressViewAndAcceptDialog(views::Widget* widget) {
    views::test::WidgetDestroyedWaiter destroyed_waiter(widget);
    AcceptWidgetAndMoveForward(widget);

    // At this point, we should be in the WebAppInstallProgressView. Wait for
    // the progress view to complete, and go to the success view.
    views::View* progress_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            WebAppInstallProgressView::kProgressBarId,
            views::ElementTrackerViews::GetContextForWidget(widget));
    EXPECT_NE(progress_view, nullptr);
    views::ProgressBar* install_progress_bar =
        views::AsViewClass<views::ProgressBar>(progress_view);
    EXPECT_NE(install_progress_bar, nullptr);

    ASSERT_TRUE(base::test::RunUntil([install_progress_bar]() -> bool {
      return (install_progress_bar->GetValue() >= 0.9);
    }));

    // Wait for all installs to be completed, if in progress.
    WebAppProvider::GetForWebApps(browser()->profile())
        ->command_manager()
        .AwaitAllCommandsCompleteForTesting();

    // We should be in the "Successful" view step now. Accept it to finish
    // installation.
    AcceptWidgetAndMoveForward(widget);
    destroyed_waiter.Wait();
  }

  IconLabelBubbleView* GetPwaInstallIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    if (!browser_view || !browser_view->toolbar_button_provider()) {
      return nullptr;
    }
    return browser_view->toolbar_button_provider()->GetPageActionView(
        kActionInstallPwa);
  }

  void AcceptWidgetAndMoveForward(views::Widget* widget) {
    widget->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  }

 private:
  base::AutoReset<std::optional<base::TimeDelta>> progress_delay_override_;
  base::test::ScopedFeatureList feature_list_{features::kWebAppInstallDialog};
};

IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest, SimpleInstallFlow) {
  const GURL app_url =
      embedded_https_test_server().GetURL("/banners/manifest_test_page.html");
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), app_url));

  // Wait for the omnibox icon to become visible.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    auto* icon = GetPwaInstallIconView();
    return icon && icon->GetVisible();
  }));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");
  web_app::WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  AdvanceToDoneAndAccept(widget);

  const webapps::AppId app_id = install_observer.Wait();
  EXPECT_EQ(FindAppWithUrlInScope(app_url), app_id);
}

IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest, DetailedInstallFlow) {
  // Detailed install flow is triggered when screenshots are available.
  GURL app_url = embedded_https_test_server().GetURL(
      "/banners/"
      "manifest_test_page.html?manifest=manifest_with_screenshots.json");
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), app_url));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    auto* icon = GetPwaInstallIconView();
    return icon && icon->GetVisible();
  }));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");

  web_app::WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  AdvanceToDoneAndAccept(widget);

  const webapps::AppId app_id = install_observer.Wait();
  EXPECT_EQ(FindAppWithUrlInScope(app_url), app_id);
}

IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest, DiyInstallFlow) {
  // Navigate to a page that is not installable.
  GURL app_url = embedded_https_test_server().GetURL(
      "/banners/no_manifest_test_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");

  web_app::WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  // Show the dialog.
  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  AdvanceToDoneAndAccept(widget);

  const webapps::AppId app_id = install_observer.Wait();
  EXPECT_EQ(FindAppWithUrlInScope(app_url), app_id);
}

IN_PROC_BROWSER_TEST_F(WebAppInstallFlowBrowserTest,
                       InstallFlowShowsIntentPickerOnClose) {
  const GURL app_url =
      embedded_https_test_server().GetURL("/banners/manifest_test_page.html");
  ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(), app_url));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    auto* icon = GetPwaInstallIconView();
    return icon && icon->GetVisible();
  }));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");
  web_app::WebAppTestInstallObserver install_observer(profile());
  install_observer.BeginListening();

  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  auto* dialog_delegate = widget->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);
  dialog_delegate->AcceptDialog();

  if (ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
          WebAppInstallFlowDialogDelegate::kOptionsViewId)) {
    dialog_delegate->AcceptDialog();
  }

  // Wait for the progress view to complete and reach the success view.
  views::View* progress_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          WebAppInstallProgressView::kProgressBarId,
          views::ElementTrackerViews::GetContextForWidget(widget));
  ASSERT_NE(progress_view, nullptr);

  views::ProgressBar* install_progress_bar =
      views::AsViewClass<views::ProgressBar>(progress_view);
  ASSERT_NE(install_progress_bar, nullptr);

  ASSERT_TRUE(base::test::RunUntil([install_progress_bar]() -> bool {
    return (install_progress_bar->GetValue() >= 0.9);
  }));

  // Wait for all background install commands to complete.
  WebAppProvider::GetForWebApps(browser()->profile())
      ->command_manager()
      .AwaitAllCommandsCompleteForTesting();

  dialog_delegate->CancelDialog();
  ASSERT_TRUE(web_app::WaitForIntentPickerToShow(browser()));
  EXPECT_TRUE(
      web_app::GetIntentPickerButton(browser()->GetBrowserForMigrationOnly())
          ->GetVisible());
}
#if BUILDFLAG(IS_WIN)

enum class CheckboxOptions { kNeither, kShortcutOnly, kTaskbarOnly, kBoth };

std::string CheckboxOptionsToString(CheckboxOptions options) {
  switch (options) {
    case CheckboxOptions::kNeither:
      return "Neither";
    case CheckboxOptions::kShortcutOnly:
      return "CreateShortcutOnly";
    case CheckboxOptions::kTaskbarOnly:
      return "PinTaskbarOnly";
    case CheckboxOptions::kBoth:
      return "Both";
  }
  NOTREACHED();
}

class WebAppInstallFlowOptionsViewTest
    : public WebAppInstallFlowBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<InstallDialogType, CheckboxOptions>> {
 public:
  void TearDownOnMainThread() override {
    web_app::test::UninstallAllWebApps(browser()->profile());
    WebAppBrowserTestBase::TearDownOnMainThread();
  }

  GURL GetCurrentAppUrlForFlow() {
    InstallDialogType flow_type = std::get<InstallDialogType>(GetParam());
    switch (flow_type) {
      case InstallDialogType::kSimple:
        return embedded_https_test_server().GetURL(
            "/banners/manifest_test_page.html");
      case InstallDialogType::kDetailed:
        return embedded_https_test_server().GetURL(
            "/banners/"
            "manifest_test_page.html?manifest=manifest_with_screenshots.json");
      case InstallDialogType::kDiy:
        return embedded_https_test_server().GetURL(
            "/banners/no_manifest_test_page.html");
    }
    NOTREACHED();
  }

  // Title is obtained from the manifest.json used in the corresponding url
  // returned by `GetCurrentAppUrlForFlow()`.
  std::string GetCurrentAppTitleForFlow() {
    InstallDialogType flow_type = std::get<InstallDialogType>(GetParam());
    switch (flow_type) {
      case InstallDialogType::kSimple:
        return "Manifest test app";
      case InstallDialogType::kDetailed:
        return "PWA Bottom Sheet";
      case InstallDialogType::kDiy:
        return "Web app banner test page";
    }
    NOTREACHED();
  }
};

IN_PROC_BROWSER_TEST_P(WebAppInstallFlowOptionsViewTest, OptionsParameters) {
  base::ScopedAllowBlockingForTesting allow_blocking_for_files;
  const auto& [flow_type, options] = GetParam();

  bool create_shortcut = false;
  bool pin_to_taskbar = false;

  switch (options) {
    case CheckboxOptions::kNeither:
      create_shortcut = false;
      pin_to_taskbar = false;
      break;
    case CheckboxOptions::kShortcutOnly:
      create_shortcut = true;
      pin_to_taskbar = false;
      break;
    case CheckboxOptions::kTaskbarOnly:
      create_shortcut = false;
      pin_to_taskbar = true;
      break;
    case CheckboxOptions::kBoth:
      create_shortcut = true;
      pin_to_taskbar = true;
      break;
  }

  // Navigation and wait logic depends on flow type
  if (flow_type == InstallDialogType::kDiy) {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GetCurrentAppUrlForFlow()));
  } else {
    ASSERT_TRUE(NavigateAndAwaitInstallabilityCheck(browser(),
                                                    GetCurrentAppUrlForFlow()));
    ASSERT_TRUE(base::test::RunUntil([&]() {
      auto* icon = GetPwaInstallIconView();
      return icon && icon->GetVisible();
    }));
  }

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "WebAppInstallFlowDialog");
  web_app::WebAppTestInstallWithOsHooksObserver install_observer(profile());
  install_observer.BeginListening();

  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);

  views::Widget* widget = waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);
  AcceptWidgetAndMoveForward(widget);

  // Use Element identifiers to find checkboxes and set their state.
  views::View* shortcut_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          web_app::WebAppInstallOptionsView::kCreateShortcutCheckboxId,
          views::ElementTrackerViews::GetContextForWidget(widget));
  ASSERT_NE(shortcut_view, nullptr);
  auto* shortcut_checkbox = views::AsViewClass<views::Checkbox>(shortcut_view);
  ASSERT_NE(shortcut_checkbox, nullptr);
  shortcut_checkbox->SetChecked(create_shortcut);

  views::View* taskbar_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          web_app::WebAppInstallOptionsView::kPinToTaskbarCheckboxId,
          views::ElementTrackerViews::GetContextForWidget(widget));
  ASSERT_NE(taskbar_view, nullptr);
  auto* taskbar_checkbox = views::AsViewClass<views::Checkbox>(taskbar_view);
  ASSERT_NE(taskbar_checkbox, nullptr);
  taskbar_checkbox->SetChecked(pin_to_taskbar);

  ForwardThroughProgressViewAndAcceptDialog(widget);

  const webapps::AppId app_id = install_observer.Wait();

  // Verify OS hooks based on test parameters.
  // We cannot use "IsShortcutCreated()" here, since that always returns true,
  // as a shortcut is always created on the "Start Menu".
  base::FilePath desktop_shortcut_path =
      os_integration_override().GetShortcutPath(
          profile(), os_integration_override().desktop(), app_id,
          GetCurrentAppTitleForFlow());
  EXPECT_EQ(base::PathExists(desktop_shortcut_path), create_shortcut);
  EXPECT_EQ(os_integration_override().IsAppPinnedToTaskbar(app_id),
            pin_to_taskbar);

  test::UninstallWebApp(profile(), app_id);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppInstallFlowOptionsViewTest,
    testing::Combine(testing::Values(InstallDialogType::kSimple,
                                     InstallDialogType::kDetailed,
                                     InstallDialogType::kDiy),
                     testing::Values(CheckboxOptions::kNeither,
                                     CheckboxOptions::kShortcutOnly,
                                     CheckboxOptions::kTaskbarOnly,
                                     CheckboxOptions::kBoth)),
    [](const ::testing::TestParamInfo<
        std::tuple<InstallDialogType, CheckboxOptions>>& test_info) {
      return base::StrCat(
          {base::ToString(std::get<InstallDialogType>(test_info.param)), "_",
           CheckboxOptionsToString(
               std::get<CheckboxOptions>(test_info.param))});
    });

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

}  // namespace web_app
