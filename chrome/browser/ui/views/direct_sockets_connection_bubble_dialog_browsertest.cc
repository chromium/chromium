// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

namespace chrome {

namespace {

const char kHostname[] = "ds.example";

}  // anonymous namespace

class DirectSocketsConnectionBubbleDialogBrowserTest
    : public DialogBrowserTest {
 public:
  DirectSocketsConnectionBubbleDialogBrowserTest() = default;

  DirectSocketsConnectionBubbleDialogBrowserTest(
      const DirectSocketsConnectionBubbleDialogBrowserTest&) = delete;
  DirectSocketsConnectionBubbleDialogBrowserTest& operator=(
      const DirectSocketsConnectionBubbleDialogBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    chrome::ShowDirectSocketsConnectionDialog(
        browser(), kHostname,
        base::BindOnce(
            &DirectSocketsConnectionBubbleDialogBrowserTest::OnDialogProceed,
            weak_ptr_factory_.GetWeakPtr(), name));
  }

 private:
  void OnDialogProceed(const std::string& name,
                       bool accepted,
                       const std::string& address,
                       const std::string& port) {
    // The Dialog is closed by default. Make sure the dialog isn't accepted and
    // what the user has input should not be sent back to the caller.
    if (name == "default") {
      EXPECT_FALSE(accepted);
      EXPECT_TRUE(address.empty());
      EXPECT_TRUE(port.empty());
    }
  }

  base::WeakPtrFactory<DirectSocketsConnectionBubbleDialogBrowserTest>
      weak_ptr_factory_{this};
};

// Test that calls ShowUi("default").
IN_PROC_BROWSER_TEST_F(DirectSocketsConnectionBubbleDialogBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

}  // namespace chrome
