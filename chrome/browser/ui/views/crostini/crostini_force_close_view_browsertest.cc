// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_force_close_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/views/crostini/crostini_dialogue_browser_test_util.h"
#include "components/exo/shell_surface.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/wm_helper.h"
#include "content/public/test/browser_test.h"
#include "ui/views/controls/button/label_button.h"

namespace crostini {
namespace {

class CrostiniForceCloseViewTest : public DialogBrowserTest {
 public:
  CrostiniForceCloseViewTest() : weak_ptr_factory_(this) {}

  CrostiniForceCloseViewTest(const CrostiniForceCloseViewTest&) = delete;
  CrostiniForceCloseViewTest& operator=(const CrostiniForceCloseViewTest&) =
      delete;

  void ShowUi(const std::string& name) override {
    wm_helper_ = std::make_unique<exo::WMHelper>();
    closable_surface_ =
        exo::test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
    closable_surface_->root_surface()->Commit();
    closable_widget_ = closable_surface_->GetWidget();

    dialog_widget_ = CrostiniForceCloseView::Show(
        "Test App", closable_widget_,
        base::BindOnce(&CrostiniForceCloseViewTest::ForceCloseCounter,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void TearDownOnMainThread() override {
    dialog_widget_ = nullptr;
    closable_widget_ = nullptr;
    closable_surface_.reset();
    wm_helper_.reset();
  }

 protected:
  // This method is used as the force close callback, allowing us to observe
  // calls to it.
  void ForceCloseCounter() { force_close_invocations_++; }

  std::unique_ptr<exo::WMHelper> wm_helper_;

  std::unique_ptr<exo::ShellSurface> closable_surface_;
  raw_ptr<views::Widget, DanglingUntriaged> closable_widget_;

  raw_ptr<views::Widget, DanglingUntriaged> dialog_widget_ = nullptr;

  int force_close_invocations_ = 0;

  base::WeakPtrFactory<CrostiniForceCloseViewTest> weak_ptr_factory_;
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
