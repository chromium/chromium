// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_bubble_views.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/test/browser_test.h"

namespace {

class ConfirmBubbleTestModel : public ConfirmBubbleModel {
  std::u16string GetTitle() const override { return u"Test dialog"; }
  std::u16string GetMessageText() const override {
    return u"A very long message which should be forced to wrap when displayed "
           u"in the confirm bubble; this can be used to verify proper "
           u"positioning of text with respect to the bubble bounds and other "
           u"elements.";
  }
};

}  // namespace

class ConfirmBubbleTest : public DialogBrowserTest {
 public:
  ConfirmBubbleTest() = default;
  ConfirmBubbleTest(const ConfirmBubbleTest&) = delete;
  ConfirmBubbleTest& operator=(const ConfirmBubbleTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    constrained_window::CreateBrowserModalDialogViews(
        new ConfirmBubbleViews(std::make_unique<ConfirmBubbleTestModel>()),
        browser()->window()->GetNativeWindow())
        ->Show();
  }
};

IN_PROC_BROWSER_TEST_F(ConfirmBubbleTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
