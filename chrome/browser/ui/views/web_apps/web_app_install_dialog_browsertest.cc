// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_split.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_dialog_delegate.h"
#include "chrome/browser/ui/views/web_apps/web_app_install_flow_dialog_delegate.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/ui_manager/update_dialog_types.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

namespace web_app {

class WebAppInstallDialogBrowserTest : public DialogBrowserTest {
 public:
  WebAppInstallDialogBrowserTest() {
    // TODO(b/492657940): Remove this once the feature is launched.
    feature_list_.InitAndEnableFeature(features::kWebAppInstallDialog);
  }
  WebAppInstallDialogBrowserTest(const WebAppInstallDialogBrowserTest&) =
      delete;
  WebAppInstallDialogBrowserTest& operator=(
      const WebAppInstallDialogBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto install_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("https://example.com"));
    install_info->title = u"Test App";
    auto icon = ::shortcuts::GenerateBitmap(kIconSize, u"A");
    install_info->icon_bitmaps.any[kIconSize] = icon;
    install_info->trusted_icon_bitmaps.any[kIconSize] = icon;
    install_info->description = u"This is a test app";

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "WebAppInstallFlowDialog");

    auto* promoter = webapps::MLInstallabilityPromoter::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    auto install_tracker = promoter->RegisterCurrentInstallForWebContents(
        webapps::WebappInstallSource::MENU_BROWSER_TAB);

    WebAppInstallFlowDialogDelegate::Show(
        browser()->tab_strip_model()->GetActiveWebContents(),
        std::move(install_info), std::move(install_tracker),
        base::BindOnce(&WebAppInstallDialogBrowserTest::OnDialogCompleted,
                       weak_ptr_factory_.GetWeakPtr()),
        PwaInProductHelpState::kNotShown, /*screenshot_fetcher=*/{},
        /*show_initiating_origin=*/false, InstallDialogType::kSimple);

    views::Widget* widget = waiter.WaitIfNeededAndGet();
    ASSERT_NE(nullptr, widget);

    base::WeakPtr<views::Widget> weak_widget = widget->GetWeakPtr();
    views::test::WidgetDestroyedWaiter destruction_waiter(widget);

    std::vector<std::string> parts = base::SplitString(
        name, "_", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    // Iterate through the parts and perform the corresponding action.
    for (const std::string& part : parts) {
      if (!weak_widget) {
        break;
      }
      if (part == "Accept") {
        weak_widget->widget_delegate()->AsDialogDelegate()->AcceptDialog();
        if (weak_widget && weak_widget->IsClosed()) {
          destruction_waiter.Wait();
          dialog_closed_ = true;
        }
      } else if (part == "Cancel") {
        weak_widget->widget_delegate()->AsDialogDelegate()->CancelDialog();
        if (weak_widget && weak_widget->IsClosed()) {
          destruction_waiter.Wait();
          dialog_closed_ = true;
        }
      } else if (part == "OsWindows") {
        // TODO: Configure Windows-specific dialog state if needed.
      }
    }
  }

  void OnDialogCompleted(bool accepted,
                         std::unique_ptr<WebAppInstallInfo> install_info) {
    dialog_accepted_ = accepted;
    dialog_install_info_ = std::move(install_info);
  }

  bool VerifyUi() override {
    // If the dialog was closed (e.g. accepted or cancelled), we don't need to
    // verify the UI, because the dialog is gone.
    if (dialog_closed_) {
      return true;
    }
    return DialogBrowserTest::VerifyUi();
  }

 protected:
  std::optional<bool> dialog_accepted_;
  std::unique_ptr<WebAppInstallInfo> dialog_install_info_;
  bool dialog_closed_ = false;

 private:
  base::test::ScopedFeatureList feature_list_;
  base::WeakPtrFactory<WebAppInstallDialogBrowserTest> weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_F(WebAppInstallDialogBrowserTest, InvokeUi_OsWindows) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppInstallDialogBrowserTest,
                       InvokeUi_OsWindows_Accept) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppInstallDialogBrowserTest,
                       InvokeUi_OsWindows_Accept_Accept) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppInstallDialogBrowserTest,
                       InvokeUi_OsWindows_Accept_Accept_Accept) {
  ShowAndVerifyUi();
}

// TODO(b/492657940): Failing, fix.
IN_PROC_BROWSER_TEST_F(
    WebAppInstallDialogBrowserTest,
    DISABLED_InvokeUi_OsWindows_Accept_Accept_Accept_Accept) {
  ShowAndVerifyUi();
}

// TODO(b/492657940): Failing, fix.
IN_PROC_BROWSER_TEST_F(WebAppInstallDialogBrowserTest,
                       DISABLED_InvokeUi_OsWindows_Cancel) {
  ShowAndVerifyUi();
}

}  // namespace web_app
