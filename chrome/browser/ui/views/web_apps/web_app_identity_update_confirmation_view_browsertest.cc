// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/root_view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#endif

namespace {

// The length of the App Identity Update dialog explanation message.
constexpr int kMessageHeaderLength = 88;

}  // namespace

class WebAppIdentityUpdateConfirmationViewBrowserTest
    : public DialogBrowserTest {
 public:
  WebAppIdentityUpdateConfirmationViewBrowserTest() = default;
  WebAppIdentityUpdateConfirmationViewBrowserTest(
      const WebAppIdentityUpdateConfirmationViewBrowserTest&) = delete;
  WebAppIdentityUpdateConfirmationViewBrowserTest& operator=(
      const WebAppIdentityUpdateConfirmationViewBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    provider_ = web_app::WebAppProvider::GetForTest(browser()->profile());
    DCHECK(provider_);
  }

  void TearDownOnMainThread() override { provider_ = nullptr; }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    gfx::ImageSkia image;
    auto* bitmap = image.bitmap();
    std::u16string old_name = u"Old App Title";
    std::u16string new_name = title_change_ ? u"New App Title" : old_name;
    web_app::ShowWebAppIdentityUpdateDialog(
        app_id_, title_change_, icon_change_, old_name, new_name, *bitmap,
        *bitmap, browser()->tab_strip_model()->GetActiveWebContents(),
        base::DoNothing());
  }

  bool VerifyUi() override {
    if (!DialogBrowserTest::VerifyUi())
      return false;

    views::Widget::Widgets widgets;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    for (aura::Window* root_window : ash::Shell::GetAllRootWindows())
      views::Widget::GetAllChildWidgets(root_window, &widgets);
#else
    widgets = views::test::WidgetTest::GetAllWidgets();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    for (views::Widget* widget : widgets) {
      if (!widget->GetRootView())
        continue;
      views::View* view = widget->GetRootView()->GetViewByID(
          VIEW_ID_APP_IDENTITY_UPDATE_HEADER);
      if (view) {
        views::Label* label = (views::Label*)view;
        return (icon_change_ ? kMessageHeaderLength : 0) ==
               label->GetText().size();
      }
    }

    // The correct view was not found.
    return false;
  }

 protected:
  raw_ptr<web_app::WebAppProvider> provider_ = nullptr;

  // Whether the UI should show that the title has changed.
  bool title_change_ = true;

  // Whether the UI should show that the app icon has changed.
  bool icon_change_ = true;

  std::string app_id_;
};

IN_PROC_BROWSER_TEST_F(WebAppIdentityUpdateConfirmationViewBrowserTest,
                       InvokeUi_title_change) {
  app_id_ = "TestAppIdentity";
  title_change_ = true;
  icon_change_ = false;
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppIdentityUpdateConfirmationViewBrowserTest,
                       InvokeUi_icon_change) {
  app_id_ = "TestAppIdentity";
  title_change_ = false;
  icon_change_ = true;
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppIdentityUpdateConfirmationViewBrowserTest,
                       InvokeUi_both_change) {
  app_id_ = "TestAppIdentity";
  title_change_ = true;
  icon_change_ = true;
  ShowAndVerifyUi();
}

// This test verifies that the App Identity Update dialog closes if the app that
// was asking for an identity change is uninstalled while the dialog is open.
// Disabled due to flake. https://crbug.com/1347280
IN_PROC_BROWSER_TEST_F(WebAppIdentityUpdateConfirmationViewBrowserTest,
                       DISABLED_CloseAppIdUpdateDialogOnUninstall) {
  views::NamedWidgetShownWaiter app_id_waiter(
      views::test::AnyWidgetTestPasskey(),
      "WebAppIdentityUpdateConfirmationView");

  app_id_ = web_app::test::InstallDummyWebApp(browser()->profile(), "Web App",
                                              GURL("http://some.url"));
  ShowUi("WebAppIdentityUpdateConfirmationView");

  views::Widget* dialog_widget = app_id_waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(dialog_widget != nullptr);
  ASSERT_FALSE(dialog_widget->IsClosed());

  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
  base::RunLoop run_loop;
  observer.set_closing_callback(
      base::BindLambdaForTesting([&](views::Widget* widget) {
        if (widget == dialog_widget)
          run_loop.Quit();
      }));
  // Uninstalling the app will abort its App Identity Update dialog.
  web_app::test::UninstallWebApp(browser()->profile(), app_id_);
  run_loop.Run();
}
