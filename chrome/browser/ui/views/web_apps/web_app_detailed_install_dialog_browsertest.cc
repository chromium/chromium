// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "components/webapps/common/constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace web_app {

class WebAppDetailedInstallDialogBrowserTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto install_info = std::make_unique<WebAppInstallInfo>(
        GenerateManifestIdFromStartUrlOnly(GURL("https://example.com")));
    install_info->title = u"test";
    install_info->description = u"This is a test app";
    install_info->start_url = GURL("https://example.com");

    install_info->icon_bitmaps.any[kIconSize] =
        CreateSolidColorIcon(kIconSize, kIconSize, kIconColor);

    std::vector<webapps::Screenshot> screenshots;
    if (name == "single_screenshot") {
      screenshots.emplace_back(
          CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorGREEN),
          u"example screenshot");
    } else if (name == "multiple_screenshots") {
      screenshots.emplace_back(
          CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorGREEN),
          u"example screenshot");
      screenshots.emplace_back(
          CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorBLACK),
          u"example screenshot 2");
      screenshots.emplace_back(
          CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorBLUE),
          u"");
    } else if (name == "max_ratio_screenshot") {
      screenshots.emplace_back(
          CreateSolidColorIcon(
              webapps::kMaximumScreenshotRatio * kScreenshotSize,
              kScreenshotSize, SK_ColorGREEN),
          absl::nullopt);
    } else {
      screenshots.emplace_back(
          CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorGREEN),
          absl::nullopt);
    }

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
        webapps::MLInstallabilityPromoter::FromWebContents(web_contents)
            ->RegisterCurrentInstallForWebContents(
                webapps::WebappInstallSource::MENU_CREATE_SHORTCUT);

    ShowWebAppDetailedInstallDialog(
        browser()->tab_strip_model()->GetWebContentsAt(0),
        std::move(install_info), std::move(install_tracker),
        base::BindLambdaForTesting(
            [&](bool result, std::unique_ptr<WebAppInstallInfo>) {
              dialog_accepted_ = result;
            }),
        screenshots, PwaInProductHelpState::kNotShown);
  }
  absl::optional<bool> dialog_accepted() { return dialog_accepted_; }

 private:
  SkBitmap CreateSolidColorIcon(int width, int height, SkColor color) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(color);
    return bitmap;
  }

  static constexpr int kIconSize = 40;
  static constexpr int kScreenshotSize = 300;
  static constexpr SkColor kIconColor = SK_ColorGREEN;
  absl::optional<bool> dialog_accepted_ = absl::nullopt;
};

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_single_screenshot) {
  ShowAndVerifyUi();
  // `ShowAndVerifyUi` close the dialog without accept/cancel.
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_multiple_screenshots) {
  ShowAndVerifyUi();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_max_ratio_screenshot) {
  ShowAndVerifyUi();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_widget_destroyed_on_navigation) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("destroyed_on_navigation");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL(url::kAboutBlankURL), /*number_of_navigations=*/1);

  destroy_waiter.Wait();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_widget_destroyed_on_web_contents_invisible) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("destroyed_on_web_contents_invisible");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  // Navigate to a new tab.
  content::WebContents::CreateParams params(browser()->profile());
  browser()->tab_strip_model()->AppendWebContents(
      content::WebContents::Create(params), /*foreground=*/true);

  destroy_waiter.Wait();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_widget_destroyed_on_web_contents_destroyed) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("destroyed_on_web_contents_destroyed");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  content::WebContents::CreateParams params(browser()->profile());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->Close();

  destroy_waiter.Wait();
  ASSERT_FALSE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_accept_dialog) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("accept_dialog");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::AcceptDialog(widget);
  destroy_waiter.Wait();
  ASSERT_TRUE(dialog_accepted().value());
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_cancel_dialog) {
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppDetailedInstallDialog");
  ShowUi("cancel_dialog");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::CancelDialog(widget);
  destroy_waiter.Wait();
  ASSERT_FALSE(dialog_accepted().value());
}

}  // namespace web_app
