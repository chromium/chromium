// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_installer_view.h"

#include "base/bind.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager_mock.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_task.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

using ::testing::_;
using InstallationResult = borealis::BorealisInstallResult;

namespace borealis {
namespace {

class BorealisInstallerMock : public borealis::BorealisInstaller {
 public:
  MOCK_METHOD0(IsProcessing, bool());
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD(void,
              Uninstall,
              (base::OnceCallback<void(BorealisUninstallResult)>),
              ());
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
};

class BorealisInstallerViewBrowserTest : public DialogBrowserTest {
 public:
  BorealisInstallerViewBrowserTest() = default;

  // DialogBrowserTest:
  void SetUpOnMainThread() override {
    app_name_ = l10n_util::GetStringUTF16(IDS_BOREALIS_APP_NAME);

    BorealisServiceFake* fake_service =
        BorealisServiceFake::UseFakeForTesting(browser()->profile());
    fake_service->SetContextManagerForTesting(&mock_context_manager_);
    fake_service->SetInstallerForTesting(&mock_installer_);
  }

  void ShowUi(const std::string& name) override {
    borealis::ShowBorealisInstallerView(browser()->profile());
    view_ = BorealisInstallerView::GetActiveViewForTesting();
    EXPECT_FALSE(view_->GetWidget()->IsClosed());
    ExpectConfirmationDisplayed();
  }

 protected:
  bool HasAcceptButton() { return view_->GetOkButton() != nullptr; }

  bool HasCancelButton() { return view_->GetCancelButton() != nullptr; }

  void ExpectConfirmationDisplayed() {
    EXPECT_TRUE(HasAcceptButton());
    EXPECT_TRUE(HasCancelButton());
    EXPECT_EQ(view_->GetPrimaryMessage(),
              l10n_util::GetStringFUTF16(
                  IDS_BOREALIS_INSTALLER_CONFIRMATION_TITLE, app_name_));
  }

  void ExpectInstallationInProgress() {
    EXPECT_FALSE(HasAcceptButton());
    EXPECT_TRUE(HasCancelButton());
    EXPECT_EQ(view_->GetPrimaryMessage(),
              l10n_util::GetStringFUTF16(
                  IDS_BOREALIS_INSTALLER_ENVIRONMENT_SETTING_TITLE, app_name_));
  }

  void ExpectInstallationFailedWithRetry() {
    EXPECT_TRUE(HasAcceptButton());
    EXPECT_TRUE(HasCancelButton());
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
              l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_RETRY_BUTTON));
    EXPECT_EQ(view_->GetPrimaryMessage(),
              l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_TITLE));
  }

  void ExpectInstallationFailedWithNoRetry() {
    EXPECT_FALSE(HasAcceptButton());
    EXPECT_TRUE(HasCancelButton());
  }

  void ExpectInstallationCompletedSucessfully() {
    EXPECT_TRUE(HasAcceptButton());
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
              l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_LAUNCH_BUTTON));
    EXPECT_TRUE(HasCancelButton());
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL),
              l10n_util::GetStringUTF16(IDS_APP_CLOSE));
    EXPECT_EQ(view_->GetPrimaryMessage(),
              l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_FINISHED_TITLE));
  }

  void AcceptInstallation() {
    EXPECT_CALL(mock_installer_, AddObserver(_));
    EXPECT_CALL(mock_installer_, Start());
    view_->AcceptDialog();
    view_->SetInstallingStateForTesting(
        borealis::BorealisInstaller::InstallingState::kInstallingDlc);
    ExpectInstallationInProgress();
  }

  void ClickCancel() {
    EXPECT_CALL(mock_installer_, RemoveObserver(_));
    view_->CancelDialog();

    EXPECT_TRUE(view_->GetWidget()->IsClosed());
  }

  ::testing::StrictMock<BorealisInstallerMock> mock_installer_;
  ::testing::StrictMock<BorealisContextManagerMock> mock_context_manager_;
  BorealisInstallerView* view_;
  std::u16string app_name_;

 private:
  // Disallow copy and assign.
  BorealisInstallerViewBrowserTest(const BorealisInstallerViewBrowserTest&) =
      delete;
  BorealisInstallerViewBrowserTest& operator=(
      const BorealisInstallerViewBrowserTest&) = delete;
};

// Test that the dialog can be launched.
IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest, InvokeUi_default) {
  EXPECT_CALL(mock_installer_, RemoveObserver(_));
  EXPECT_CALL(mock_installer_, Cancel());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest, SucessfulInstall) {
  ShowUi("default");
  AcceptInstallation();

  view_->OnInstallationEnded(InstallationResult::kSuccess);
  ExpectInstallationCompletedSucessfully();

  EXPECT_CALL(mock_context_manager_, StartBorealis(_));
  EXPECT_CALL(mock_installer_, RemoveObserver(_));
  view_->AcceptDialog();

  EXPECT_TRUE(view_->GetWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest,
                       ConfirmationCancelled) {
  ShowUi("default");

  EXPECT_CALL(mock_installer_, Cancel());
  ClickCancel();
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest,
                       InstallationCancelled) {
  ShowUi("default");
  AcceptInstallation();

  EXPECT_CALL(mock_installer_, Cancel());
  ClickCancel();
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest,
                       InstallationSucessAfterRetry) {
  InstallationResult error_type =
      InstallationResult::kBorealisInstallInProgress;
  ShowUi("default");
  AcceptInstallation();

  view_->OnInstallationEnded(error_type);
  ExpectInstallationFailedWithRetry();
  EXPECT_EQ(view_->GetSecondaryMessage(),
            l10n_util::GetStringFUTF16(
                IDS_BOREALIS_INSTALLER_IN_PROGRESS_ERROR_MESSAGE, app_name_));

  AcceptInstallation();

  view_->OnInstallationEnded(InstallationResult::kSuccess);
  ExpectInstallationCompletedSucessfully();

  EXPECT_CALL(mock_context_manager_, StartBorealis(_));
  EXPECT_CALL(mock_installer_, RemoveObserver(_));
  view_->AcceptDialog();

  EXPECT_TRUE(view_->GetWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest, InProgressError) {
  InstallationResult error_type =
      InstallationResult::kBorealisInstallInProgress;
  ShowUi("default");
  AcceptInstallation();

  view_->OnInstallationEnded(error_type);
  ExpectInstallationFailedWithRetry();
  EXPECT_EQ(view_->GetSecondaryMessage(),
            l10n_util::GetStringFUTF16(
                IDS_BOREALIS_INSTALLER_IN_PROGRESS_ERROR_MESSAGE, app_name_));

  ClickCancel();
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest, NotAllowedError) {
  InstallationResult error_type = InstallationResult::kBorealisNotAllowed;
  ShowUi("default");
  AcceptInstallation();

  view_->OnInstallationEnded(error_type);
  ExpectInstallationFailedWithNoRetry();
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringFUTF16(IDS_BOREALIS_INSTALLER_NOT_ALLOWED_TITLE,
                                       app_name_));
  EXPECT_EQ(view_->GetSecondaryMessage(),
            l10n_util::GetStringFUTF16(
                IDS_BOREALIS_INSTALLER_NOT_ALLOWED_MESSAGE, app_name_,
                base::NumberToString16(
                    static_cast<std::underlying_type_t<InstallationResult>>(
                        error_type))));

  ClickCancel();
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest, DlcUnsupportedError) {
  InstallationResult error_type = InstallationResult::kDlcUnsupportedError;
  ShowUi("default");
  AcceptInstallation();

  view_->OnInstallationEnded(error_type);
  ExpectInstallationFailedWithNoRetry();
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringFUTF16(IDS_BOREALIS_INSTALLER_NOT_ALLOWED_TITLE,
                                       app_name_));
  EXPECT_EQ(view_->GetSecondaryMessage(),
            l10n_util::GetStringFUTF16(
                IDS_BOREALIS_INSTALLER_NOT_ALLOWED_MESSAGE, app_name_,
                base::NumberToString16(
                    static_cast<std::underlying_type_t<InstallationResult>>(
                        error_type))));

  ClickCancel();
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest, DlcInternalError) {
  InstallationResult error_type = InstallationResult::kDlcInternalError;
  ShowUi("default");
  AcceptInstallation();

  view_->OnInstallationEnded(error_type);
  ExpectInstallationFailedWithRetry();
  EXPECT_EQ(
      view_->GetSecondaryMessage(),
      l10n_util::GetStringUTF16(IDS_BOREALIS_DLC_INTERNAL_FAILED_MESSAGE));

  ClickCancel();
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest, DlcBusyError) {
  InstallationResult error_type = InstallationResult::kDlcBusyError;
  ShowUi("default");
  AcceptInstallation();

  view_->OnInstallationEnded(error_type);
  ExpectInstallationFailedWithRetry();
  EXPECT_EQ(view_->GetSecondaryMessage(),
            l10n_util::GetStringFUTF16(IDS_BOREALIS_DLC_BUSY_FAILED_MESSAGE,
                                       app_name_));

  ClickCancel();
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest, DlcNeedRebootError) {
  InstallationResult error_type = InstallationResult::kDlcNeedRebootError;
  ShowUi("default");
  AcceptInstallation();

  view_->OnInstallationEnded(error_type);
  ExpectInstallationFailedWithRetry();
  EXPECT_EQ(view_->GetSecondaryMessage(),
            l10n_util::GetStringFUTF16(
                IDS_BOREALIS_DLC_NEED_REBOOT_FAILED_MESSAGE, app_name_));

  ClickCancel();
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest, DlcNeedSpaceError) {
  InstallationResult error_type = InstallationResult::kDlcNeedSpaceError;
  ShowUi("default");
  AcceptInstallation();

  view_->OnInstallationEnded(error_type);
  ExpectInstallationFailedWithRetry();
  EXPECT_EQ(
      view_->GetSecondaryMessage(),
      l10n_util::GetStringUTF16(IDS_BOREALIS_INSUFFICIENT_DISK_SPACE_MESSAGE));

  ClickCancel();
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest, DlcNeedUpdateError) {
  InstallationResult error_type = InstallationResult::kDlcNeedUpdateError;
  ShowUi("default");
  AcceptInstallation();

  view_->OnInstallationEnded(error_type);
  ExpectInstallationFailedWithNoRetry();
  EXPECT_EQ(view_->GetPrimaryMessage(),
            l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ERROR_TITLE));
  EXPECT_EQ(
      view_->GetSecondaryMessage(),
      l10n_util::GetStringUTF16(IDS_BOREALIS_DLC_NEED_UPDATE_FAILED_MESSAGE));

  ClickCancel();
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest, DlcUnknownError) {
  InstallationResult error_type = InstallationResult::kDlcUnknownError;
  ShowUi("default");
  AcceptInstallation();

  view_->OnInstallationEnded(error_type);
  ExpectInstallationFailedWithRetry();
  EXPECT_EQ(view_->GetSecondaryMessage(),
            l10n_util::GetStringFUTF16(
                IDS_BOREALIS_GENERIC_ERROR_MESSAGE, app_name_,
                base::NumberToString16(
                    static_cast<std::underlying_type_t<InstallationResult>>(
                        error_type))));

  ClickCancel();
}
}  // namespace
}  // namespace borealis
