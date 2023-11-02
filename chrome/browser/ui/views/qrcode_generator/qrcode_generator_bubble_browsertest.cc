// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_bubble.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test.h"

namespace {
class QRCodeGeneratorBubbleBrowserTest : public DialogBrowserTest {
 public:
  QRCodeGeneratorBubbleBrowserTest() = default;

  QRCodeGeneratorBubbleBrowserTest(const QRCodeGeneratorBubbleBrowserTest&) =
      delete;
  QRCodeGeneratorBubbleBrowserTest& operator=(
      const QRCodeGeneratorBubbleBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    constexpr char kErrorPath[] =
        "long_path_to_cause_error_a9g8dj98sjf0dg9jhbrsjmhrth8mr9thmddst98hmsrs9"
        "t8hjhbmrhmkfglsn45onwp3o45inupojiw4p5oiyjw4poi5ynmp4w5oinypo4wi5nmypow"
        "4n5pyoiwn54poyinw4po5iynwp4o5iynw4opi5ynpow45iunyno4i5unycw4o5iuynewo5"
        "4iunyo45uiynw4o5ynw4o5iuyno45uny54ouyno4ynw4o5ynw4o5y";
    std::string url = "https://www.chromium.org/";
    if (name != "default")
      url += kErrorPath;
    if (name == "bottom_error")
      url += "y";
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    auto* bubble_controller =
        qrcode_generator::QRCodeGeneratorBubbleController::Get(web_contents);
    bubble_controller->ShowBubble(GURL(url), true);
  }
};

IN_PROC_BROWSER_TEST_F(QRCodeGeneratorBubbleBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(QRCodeGeneratorBubbleBrowserTest, InvokeUi_top_error) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(QRCodeGeneratorBubbleBrowserTest, InvokeUi_bottom_error) {
  ShowAndVerifyUi();
}

}  // namespace
