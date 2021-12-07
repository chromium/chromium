// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/any_widget_observer.h"

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
    chrome::ShowWebAppIdentityUpdateDialog(
        app_id_, true, false, u"Old App Title", u"New App Title", *bitmap,
        *bitmap, browser()->tab_strip_model()->GetActiveWebContents(),
        base::DoNothing());
  }

 protected:
  web_app::WebAppProvider* provider_ = nullptr;

  std::string app_id_;
};

IN_PROC_BROWSER_TEST_F(WebAppIdentityUpdateConfirmationViewBrowserTest,
                       InvokeUi_default) {
  app_id_ = "TestAppIdentity";
  ShowAndVerifyUi();
}

// This test verifies that the App Identity Update dialog closes if the app that
// was asking for an identity change is uninstalled while the dialog is open.
IN_PROC_BROWSER_TEST_F(WebAppIdentityUpdateConfirmationViewBrowserTest,
                       CloseAppIdUpdateDialogOnUninstall) {
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
