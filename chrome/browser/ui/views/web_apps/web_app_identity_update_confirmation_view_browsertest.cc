// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/image/image_skia.h"

class WebAppIdentityUpdateConfirmationViewBrowserTest
    : public DialogBrowserTest {
 public:
  WebAppIdentityUpdateConfirmationViewBrowserTest() = default;
  WebAppIdentityUpdateConfirmationViewBrowserTest(
      const WebAppIdentityUpdateConfirmationViewBrowserTest&) = delete;
  WebAppIdentityUpdateConfirmationViewBrowserTest& operator=(
      const WebAppIdentityUpdateConfirmationViewBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    gfx::ImageSkia image;
    auto* bitmap = image.bitmap();
    chrome::ShowWebAppIdentityUpdateDialog(
        "TestAppIdentity", true, false, u"Old App Title", u"New App Title",
        *bitmap, *bitmap, browser()->tab_strip_model()->GetActiveWebContents(),
        base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(WebAppIdentityUpdateConfirmationViewBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
