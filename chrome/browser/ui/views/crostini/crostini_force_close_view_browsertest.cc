// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_force_close_view.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/views/crostini/crostini_dialogue_browser_test_util.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/button/label_button.h"

namespace crostini {
namespace {

class CrostiniForceCloseViewTest : public DialogBrowserTest {
 public:
  CrostiniForceCloseViewTest() : weak_ptr_factory_(this) {}

  void ShowUi(const std::string& name) override {
    dialog_widget_ = CrostiniForceCloseView::Show(
        "Test App", nullptr, nullptr,
        base::BindOnce(&CrostiniForceCloseViewTest::ForceCloseCounter,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 protected:
  // This method is used as the force close callback, allowing us to observe
  // calls to it.
  void ForceCloseCounter() { force_close_invocations_++; }

  views::Widget* dialog_widget_ = nullptr;

  int force_close_invocations_ = 0;

  base::WeakPtrFactory<CrostiniForceCloseViewTest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniForceCloseViewTest);
};

IN_PROC_BROWSER_TEST_F(CrostiniForceCloseViewTest, FocusesForceQuit) {
  ShowUi("");
  EXPECT_EQ(
      dialog_widget_->widget_delegate()->AsDialogDelegate()->GetOkButton(),
      dialog_widget_->GetFocusManager()->GetFocusedView());
}

IN_PROC_BROWSER_TEST_F(CrostiniForceCloseViewTest, OkInvokesCallback) {
  ShowUi("");
  EXPECT_EQ(force_close_invocations_, 0);
  dialog_widget_->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  EXPECT_EQ(force_close_invocations_, 1);
}

IN_PROC_BROWSER_TEST_F(CrostiniForceCloseViewTest,
                       CancelDoesNotInvokeCallback) {
  ShowUi("");
  EXPECT_EQ(force_close_invocations_, 0);
  dialog_widget_->widget_delegate()->AsDialogDelegate()->CancelDialog();
  EXPECT_EQ(force_close_invocations_, 0);
}

IN_PROC_BROWSER_TEST_F(CrostiniForceCloseViewTest, CloseDoesNotInvokeCallback) {
  ShowUi("");
  EXPECT_EQ(force_close_invocations_, 0);
  dialog_widget_->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(force_close_invocations_, 0);
}

IN_PROC_BROWSER_TEST_F(CrostiniForceCloseViewTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

}  // namespace
}  // namespace crostini
