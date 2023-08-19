// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/policy/enterprise_startup_dialog_view.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

namespace policy {
namespace {
constexpr char16_t kMessage[] = u"message";
constexpr char16_t kButton[] = u"button";

void DialogResultCallback(bool result, bool can_show_browser_window) {}
}  // namespace

class EnterpriseStartupDialogViewBrowserTest : public DialogBrowserTest {
 public:
  EnterpriseStartupDialogViewBrowserTest() = default;

  EnterpriseStartupDialogViewBrowserTest(
      const EnterpriseStartupDialogViewBrowserTest&) = delete;
  EnterpriseStartupDialogViewBrowserTest& operator=(
      const EnterpriseStartupDialogViewBrowserTest&) = delete;

  ~EnterpriseStartupDialogViewBrowserTest() override = default;

  // override DialogBrowserTest
  void ShowUi(const std::string& name) override {
    dialog =
        new EnterpriseStartupDialogView(base::BindOnce(&DialogResultCallback));
    if (name == "Information") {
      dialog->DisplayLaunchingInformationWithThrobber(kMessage);
    } else if (name == "Error") {
      dialog->DisplayErrorMessage(kMessage, kButton);
    } else if (name == "Switch") {
      dialog->DisplayLaunchingInformationWithThrobber(kMessage);
      dialog->DisplayErrorMessage(kMessage, kButton);
    }
  }

#if BUILDFLAG(IS_MAC)
  // On mac, we need to wait until the dialog launched modally before closing
  // it.
  void DismissUi() override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&EnterpriseStartupDialogView::CloseDialog,
                                  base::Unretained(dialog)));
  }
#endif

 private:
  raw_ptr<EnterpriseStartupDialogView, AcrossTasksDanglingUntriaged> dialog;
};

IN_PROC_BROWSER_TEST_F(EnterpriseStartupDialogViewBrowserTest,
                       InvokeUi_Information) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(EnterpriseStartupDialogViewBrowserTest, InvokeUi_Error) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(EnterpriseStartupDialogViewBrowserTest,
                       InvokeUi_Switch) {
  ShowAndVerifyUi();
}

}  // namespace policy
