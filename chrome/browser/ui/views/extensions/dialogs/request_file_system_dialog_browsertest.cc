// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_types.h"

namespace {

class RequestFileSystemDialogTest : public DialogBrowserTest {
 public:
  RequestFileSystemDialogTest() = default;

  RequestFileSystemDialogTest(const RequestFileSystemDialogTest&) = delete;
  RequestFileSystemDialogTest& operator=(const RequestFileSystemDialogTest&) =
      delete;

  void ShowUi(const std::string& name) override {
    extensions::ShowRequestFileSystemDialog(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "RequestFileSystemDialogTest", "TestVolume", true, base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(RequestFileSystemDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

}  // namespace
