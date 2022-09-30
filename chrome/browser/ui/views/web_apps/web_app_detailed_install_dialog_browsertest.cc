// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/common/constants.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

class WebAppDetailedInstallDialogBrowserTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto install_info = std::make_unique<WebAppInstallInfo>();
    install_info->title = u"test";
    install_info->description = u"This is a test app";

    install_info->icon_bitmaps.any[kIconSize] =
        CreateSolidColorIcon(kIconSize, kIconSize, kIconColor);

    std::vector<SkBitmap> screenshots;
    if (name == "single_screenshot") {
      screenshots.push_back(CreateSolidColorIcon(
          kScreenshotSize, kScreenshotSize, SK_ColorGREEN));
    } else if (name == "multiple_screenshots") {
      screenshots.push_back(CreateSolidColorIcon(
          kScreenshotSize, kScreenshotSize, SK_ColorGREEN));
      screenshots.push_back(CreateSolidColorIcon(
          kScreenshotSize, kScreenshotSize, SK_ColorBLACK));
      screenshots.push_back(
          CreateSolidColorIcon(kScreenshotSize, kScreenshotSize, SK_ColorBLUE));
    } else if (name == "max_ratio_screenshot") {
      screenshots.push_back(CreateSolidColorIcon(
          webapps::kMaximumScreenshotRatio * kScreenshotSize, kScreenshotSize,
          SK_ColorGREEN));
    }
    chrome::ShowWebAppDetailedInstallDialog(
        browser()->tab_strip_model()->GetWebContentsAt(0),
        std::move(install_info), base::DoNothing(), screenshots,
        chrome::PwaInProductHelpState::kNotShown);
  }

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
};

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_single_screenshot) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_multiple_screenshots) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_max_ratio_screenshot) {
  ShowAndVerifyUi();
}

}  // namespace web_app
