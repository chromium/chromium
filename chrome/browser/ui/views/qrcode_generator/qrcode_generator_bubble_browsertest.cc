// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_bubble.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_switches.h"
#include "ui/views/window/dialog_client_view.h"

namespace {
class QRCodeGeneratorBubbleBrowserTest : public DialogBrowserTest {
 public:
  QRCodeGeneratorBubbleBrowserTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    const GURL url("https://www.chromium.org");
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    auto* bubble_controller =
        qrcode_generator::QRCodeGeneratorBubbleController::Get(web_contents);
    bubble_controller->ShowBubble(url);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QRCodeGeneratorBubbleBrowserTest);
};

IN_PROC_BROWSER_TEST_F(QRCodeGeneratorBubbleBrowserTest,
                       InvokeUi_qr_generator_basic) {
  ShowAndVerifyUi();
}

}  // namespace
