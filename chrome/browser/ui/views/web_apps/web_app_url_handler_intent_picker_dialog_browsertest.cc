// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/web_app_url_handler_intent_picker_dialog_view.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/url_handler_launch_params.h"
#include "chrome/browser/web_applications/url_handler_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

namespace {

const char16_t kAppName[] = u"Test App";
const char kStartUrl[] = "https://test.com";
const char kViewClassName[] = "WebAppUrlHandlerIntentPickerView";

std::vector<web_app::UrlHandlerLaunchParams> CreateUrlHandlerLaunchParams(
    const base::FilePath& profile_path,
    const web_app::AppId& app_id) {
  std::vector<web_app::UrlHandlerLaunchParams> url_handler_matches;
  url_handler_matches.emplace_back(profile_path, app_id, GURL(kStartUrl),
                                   web_app::UrlHandlerSavedChoice::kNone,
                                   base::Time::Now());
  return url_handler_matches;
}

web_app::AppId InstallTestWebApp(Profile* profile) {
  auto app_info = std::make_unique<WebApplicationInfo>();
  app_info->start_url = GURL(kStartUrl);
  app_info->title = kAppName;
  app_info->user_display_mode = blink::mojom::DisplayMode::kStandalone;
  return web_app::test::InstallWebApp(profile, std::move(app_info));
}

views::DialogDelegate* DialogDelegateFor(views::Widget* widget) {
  auto* delegate = widget->widget_delegate()->AsDialogDelegate();
  return delegate;
}

void AutoCloseDialog(views::Widget* widget) {
  // Call CancelDialog to close the dialog, but the actual behavior will be
  // determined by the ScopedTestDialogAutoConfirm configs.
  views::test::CancelDialog(widget);
}

}  // namespace

class WebAppUrlHandlerIntentPickerDialogInProcessBrowserTest
    : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(WebAppUrlHandlerIntentPickerDialogInProcessBrowserTest,
                       ShowWebAppUrlHandlerIntentPickerDialog) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       kViewClassName);
  base::HistogramTester histogram_tester;
  web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());

  base::MockCallback<chrome::WebAppUrlHandlerAcceptanceCallback>
      show_dialog_callback;
  absl::optional<web_app::UrlHandlerLaunchParams> result_launch_params;
  bool dialog_accepted;
  ON_CALL(show_dialog_callback, Run)
      .WillByDefault([&](bool accepted,
                         absl::optional<web_app::UrlHandlerLaunchParams> data) {
        dialog_accepted = accepted;
        result_launch_params = data;
      });
  EXPECT_CALL(show_dialog_callback, Run);

  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::WEB_APP_INTENT_PICKER, KeepAliveRestartOption::DISABLED);
  WebAppUrlHandlerIntentPickerView::Show(
      GURL(kStartUrl),
      CreateUrlHandlerLaunchParams(browser()->profile()->GetPath(),
                                   test_app_id),
      std::move(keep_alive), show_dialog_callback.Get());

  waiter.WaitIfNeededAndGet()->CloseWithReason(
      views::Widget::ClosedReason::kEscKeyPressed);
  EXPECT_FALSE(dialog_accepted);
  EXPECT_FALSE(result_launch_params.has_value());
  histogram_tester.ExpectUniqueSample(
      "WebApp.UrlHandling.DialogState",
      WebAppUrlHandlerIntentPickerView::DialogState::kClosed, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppUrlHandlerIntentPickerDialogInProcessBrowserTest,
                       OpenIsDisabledByDefault) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       kViewClassName);
  base::HistogramTester histogram_tester;
  web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());

  base::MockCallback<chrome::WebAppUrlHandlerAcceptanceCallback>
      show_dialog_callback;
  absl::optional<web_app::UrlHandlerLaunchParams> result_launch_params;
  bool dialog_accepted;
  ON_CALL(show_dialog_callback, Run)
      .WillByDefault([&](bool accepted,
                         absl::optional<web_app::UrlHandlerLaunchParams> data) {
        dialog_accepted = accepted;
        result_launch_params = data;
      });
  EXPECT_CALL(show_dialog_callback, Run);

  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::CANCEL);
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::WEB_APP_INTENT_PICKER, KeepAliveRestartOption::DISABLED);
  WebAppUrlHandlerIntentPickerView::Show(
      GURL(kStartUrl),
      CreateUrlHandlerLaunchParams(browser()->profile()->GetPath(),
                                   test_app_id),
      std::move(keep_alive), show_dialog_callback.Get());

  auto* widget = waiter.WaitIfNeededAndGet();
  auto* dialog_delegate = DialogDelegateFor(widget);
  // Verify "Open" button is disabled by default.
  EXPECT_FALSE(dialog_delegate->GetOkButton()->GetEnabled());
  AutoCloseDialog(widget);
  histogram_tester.ExpectUniqueSample(
      "WebApp.UrlHandling.DialogState",
      WebAppUrlHandlerIntentPickerView::DialogState::kClosed, 1);
}

IN_PROC_BROWSER_TEST_F(WebAppUrlHandlerIntentPickerDialogInProcessBrowserTest,
                       SelectBrowser) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       kViewClassName);
  base::HistogramTester histogram_tester;
  web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());

  base::MockCallback<chrome::WebAppUrlHandlerAcceptanceCallback>
      show_dialog_callback;
  absl::optional<web_app::UrlHandlerLaunchParams> result_launch_params;
  bool dialog_accepted;
  ON_CALL(show_dialog_callback, Run)
      .WillByDefault([&](bool accepted,
                         absl::optional<web_app::UrlHandlerLaunchParams> data) {
        dialog_accepted = accepted;
        result_launch_params = data;
      });
  EXPECT_CALL(show_dialog_callback, Run);

  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION, 0);
  auto launch_params_list = CreateUrlHandlerLaunchParams(
      browser()->profile()->GetPath(), test_app_id);
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::WEB_APP_INTENT_PICKER, KeepAliveRestartOption::DISABLED);
  WebAppUrlHandlerIntentPickerView::Show(GURL(kStartUrl), launch_params_list,
                                         std::move(keep_alive),
                                         show_dialog_callback.Get());

  AutoCloseDialog(waiter.WaitIfNeededAndGet());
  EXPECT_TRUE(dialog_accepted);
  EXPECT_FALSE(result_launch_params.has_value());
  histogram_tester.ExpectUniqueSample(
      "WebApp.UrlHandling.DialogState",
      WebAppUrlHandlerIntentPickerView::DialogState::
          kBrowserAcceptedNoRememberChoice,
      1);
}

IN_PROC_BROWSER_TEST_F(WebAppUrlHandlerIntentPickerDialogInProcessBrowserTest,
                       SelectApp) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       kViewClassName);
  base::HistogramTester histogram_tester;
  web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());

  base::MockCallback<chrome::WebAppUrlHandlerAcceptanceCallback>
      show_dialog_callback;
  absl::optional<web_app::UrlHandlerLaunchParams> result_launch_params;
  bool dialog_accepted;
  ON_CALL(show_dialog_callback, Run)
      .WillByDefault([&](bool accepted,
                         absl::optional<web_app::UrlHandlerLaunchParams> data) {
        dialog_accepted = accepted;
        result_launch_params = data;
      });
  EXPECT_CALL(show_dialog_callback, Run);

  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION, 1);
  auto launch_params_list = CreateUrlHandlerLaunchParams(
      browser()->profile()->GetPath(), test_app_id);
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::WEB_APP_INTENT_PICKER, KeepAliveRestartOption::DISABLED);
  WebAppUrlHandlerIntentPickerView::Show(GURL(kStartUrl), launch_params_list,
                                         std::move(keep_alive),
                                         show_dialog_callback.Get());

  AutoCloseDialog(waiter.WaitIfNeededAndGet());
  // Select the second choice - the app.
  EXPECT_TRUE(dialog_accepted);
  EXPECT_EQ(result_launch_params, launch_params_list[0]);
  histogram_tester.ExpectUniqueSample(
      "WebApp.UrlHandling.DialogState",
      WebAppUrlHandlerIntentPickerView::DialogState::
          kAppAcceptedNoRememberChoice,
      1);
}

IN_PROC_BROWSER_TEST_F(WebAppUrlHandlerIntentPickerDialogInProcessBrowserTest,
                       FilterOutInvalidProfiles) {
  // Test valid profile path is kept.
  base::FilePath current_profile_path = browser()->profile()->GetPath();
  std::vector<web_app::UrlHandlerLaunchParams> launch_params_list =
      CreateUrlHandlerLaunchParams(current_profile_path, "app id 1");
  auto valid_profiles =
      WebAppUrlHandlerIntentPickerView::GetUrlHandlingValidProfiles(
          launch_params_list);
  EXPECT_EQ(1u, valid_profiles.size());
  EXPECT_EQ(1u, launch_params_list.size());
  EXPECT_EQ(launch_params_list.front().profile_path, current_profile_path);

  // Add an invalid profile path.
  launch_params_list.emplace_back(
      current_profile_path.Append(FILE_PATH_LITERAL("Nonexistent")), "app id 2",
      GURL(kStartUrl), web_app::UrlHandlerSavedChoice::kNone,
      base::Time::Now());
  // Verify the invalid profile is not returned.
  auto new_valid_profiles =
      WebAppUrlHandlerIntentPickerView::GetUrlHandlingValidProfiles(
          launch_params_list);
  EXPECT_EQ(1u, launch_params_list.size());
  EXPECT_EQ(1u, new_valid_profiles.size());
  EXPECT_EQ(valid_profiles, new_valid_profiles);
}

class WebAppUrlHandlerIntentPickerDialogInteractiveBrowserTest
    : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         kViewClassName);

    web_app::AppId test_app_id = InstallTestWebApp(browser()->profile());
    auto keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::WEB_APP_INTENT_PICKER,
        KeepAliveRestartOption::DISABLED);
    WebAppUrlHandlerIntentPickerView::Show(
        GURL(kStartUrl),
        CreateUrlHandlerLaunchParams(browser()->profile()->GetPath(),
                                     test_app_id),
        std::move(keep_alive), base::DoNothing());
    if (should_close_) {
      waiter.WaitIfNeededAndGet()->CloseWithReason(
          views::Widget::ClosedReason::kEscKeyPressed);
    }
  }

 protected:
  bool should_close_ = true;
};

IN_PROC_BROWSER_TEST_F(WebAppUrlHandlerIntentPickerDialogInteractiveBrowserTest,
                       InvokeUi_CloseDialog) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppUrlHandlerIntentPickerDialogInteractiveBrowserTest,
                       InvokeUi_default) {
  should_close_ = false;
  ShowAndVerifyUi();
}
