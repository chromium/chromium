// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_installer_types.mojom.h"
#include "chrome/browser/ui/views/crostini/crostini_installer_view.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/crostini/fake_crostini_installer_ui_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_browser_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/crx_file/id_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/window/dialog_client_view.h"

using crostini::mojom::InstallerError;
using crostini::mojom::InstallerState;

class CrostiniInstallerViewBrowserTest : public CrostiniDialogBrowserTest {
 public:
  CrostiniInstallerViewBrowserTest()
      : CrostiniDialogBrowserTest(false /*register_termina*/) {}

  // CrostiniDialogBrowserTest:
  void ShowUi(const std::string& name) override {
    CrostiniInstallerView::Show(browser()->profile(), &fake_delegate_);
  }

  CrostiniInstallerView* ActiveView() {
    return CrostiniInstallerView::GetActiveViewForTesting();
  }

  bool HasEnabledAcceptButton() {
    return ActiveView()->GetDialogClientView()->ok_button() != nullptr &&
           ActiveView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK);
  }

  bool HasEnabledCancelButton() {
    return ActiveView()->GetDialogClientView()->cancel_button() != nullptr &&
           ActiveView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL);
  }

 protected:
  crostini::FakeCrostiniInstallerUIDelegate fake_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniInstallerViewBrowserTest);
};

// Test the dialog is actually launched from the app launcher.
IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, InstallFlow) {
  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());
  EXPECT_EQ(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
            ActiveView()->GetDialogButtons());
  EXPECT_TRUE(HasEnabledAcceptButton());
  EXPECT_TRUE(HasEnabledCancelButton());
  EXPECT_TRUE(crostini::CrostiniManager::GetForProfile(browser()->profile())
                  ->GetInstallerViewStatus());
  EXPECT_FALSE(fake_delegate_.progress_callback_)
      << "Install() should not be called";

  ActiveView()->GetDialogClientView()->AcceptWindow();
  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed());
  EXPECT_FALSE(HasEnabledAcceptButton());
  EXPECT_TRUE(HasEnabledCancelButton());
  EXPECT_TRUE(fake_delegate_.progress_callback_)
      << "Install() should be called";
  EXPECT_TRUE(fake_delegate_.result_callback_);

  fake_delegate_.progress_callback_.Run(InstallerState::kCreateContainer, 0.4);
  fake_delegate_.progress_callback_.Run(InstallerState::kMountContainer, 0.8);
  std::move(fake_delegate_.result_callback_).Run(InstallerError::kNone);

  // This allow the dialog to be destructed.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nullptr, ActiveView());
  EXPECT_FALSE(crostini::CrostiniManager::GetForProfile(browser()->profile())
                   ->GetInstallerViewStatus());
}

IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, ErrorThenCancel) {
  ShowUi("default");
  ASSERT_NE(nullptr, ActiveView());

  ActiveView()->GetDialogClientView()->AcceptWindow();

  ASSERT_TRUE(fake_delegate_.result_callback_);
  std::move(fake_delegate_.result_callback_)
      .Run(InstallerError::kErrorCreatingDiskImage);

  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed());
  EXPECT_TRUE(HasEnabledAcceptButton());
  EXPECT_EQ(ActiveView()->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
            l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_RETRY_BUTTON));
  EXPECT_TRUE(HasEnabledCancelButton());

  ActiveView()->GetDialogClientView()->CancelWindow();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, ActiveView());

  EXPECT_FALSE(crostini::CrostiniManager::GetForProfile(browser()->profile())
                   ->GetInstallerViewStatus());
}

IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, ErrorThenRetry) {
  ShowUi("default");
  ASSERT_NE(nullptr, ActiveView());

  ActiveView()->GetDialogClientView()->AcceptWindow();

  ASSERT_TRUE(fake_delegate_.result_callback_);
  std::move(fake_delegate_.result_callback_)
      .Run(InstallerError::kErrorCreatingDiskImage);

  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed());
  EXPECT_TRUE(HasEnabledAcceptButton());
  EXPECT_EQ(ActiveView()->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
            l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_RETRY_BUTTON));

  ActiveView()->GetDialogClientView()->AcceptWindow();
  EXPECT_TRUE(fake_delegate_.result_callback_)
      << "Install() should be called again";

  std::move(fake_delegate_.result_callback_).Run(InstallerError::kNone);

  // This allow the dialog to be destructed.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nullptr, ActiveView());
  EXPECT_FALSE(crostini::CrostiniManager::GetForProfile(browser()->profile())
                   ->GetInstallerViewStatus());
}

IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, CancelBeforeStart) {
  ShowUi("default");
  ASSERT_NE(nullptr, ActiveView());
  EXPECT_TRUE(HasEnabledCancelButton());
  EXPECT_FALSE(fake_delegate_.cancel_before_start_called_);

  ActiveView()->GetDialogClientView()->CancelWindow();

  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, ActiveView());
  EXPECT_FALSE(crostini::CrostiniManager::GetForProfile(browser()->profile())
                   ->GetInstallerViewStatus());
  EXPECT_TRUE(fake_delegate_.cancel_before_start_called_);
}

IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, CancelAfterStart) {
  ShowUi("default");
  ASSERT_NE(nullptr, ActiveView());
  EXPECT_TRUE(HasEnabledAcceptButton());

  ActiveView()->GetDialogClientView()->AcceptWindow();
  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed());
  EXPECT_TRUE(HasEnabledCancelButton());
  EXPECT_TRUE(fake_delegate_.progress_callback_)
      << "Install() should be called";

  EXPECT_FALSE(fake_delegate_.cancel_callback_)
      << "Cancel() should not be called";
  ActiveView()->GetDialogClientView()->CancelWindow();
  EXPECT_TRUE(fake_delegate_.cancel_callback_) << "Cancel() should be called";
  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed())
      << "Dialog should not close before cancel callback";
  EXPECT_FALSE(HasEnabledAcceptButton());
  EXPECT_FALSE(HasEnabledCancelButton());

  std::move(fake_delegate_.cancel_callback_).Run();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, ActiveView());
  EXPECT_FALSE(crostini::CrostiniManager::GetForProfile(browser()->profile())
                   ->GetInstallerViewStatus());

  EXPECT_FALSE(fake_delegate_.cancel_before_start_called_);
}
