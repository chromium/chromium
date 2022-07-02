// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/web_apps/web_app_detailed_install_dialog.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

class WebAppDetailedInstallDialogBrowserTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto install_info = std::make_unique<WebAppInstallInfo>();
    install_info->title = u"test";
    install_info->description = u"This is a test app";

    AddGeneratedIcon(&install_info->icon_bitmaps.any, kIconSize, kIconColor);

    chrome::ShowWebAppDetailedInstallDialog(
        browser()->tab_strip_model()->GetWebContentsAt(0),
        std::move(install_info), base::DoNothing(),
        chrome::PwaInProductHelpState::kNotShown);
  }

 private:
  static constexpr int kIconSize = 40;
  static constexpr SkColor kIconColor = SK_ColorGREEN;
};

IN_PROC_BROWSER_TEST_F(WebAppDetailedInstallDialogBrowserTest,
                       InvokeUi_Default) {
  ShowAndVerifyUi();
}

}  // namespace web_app
