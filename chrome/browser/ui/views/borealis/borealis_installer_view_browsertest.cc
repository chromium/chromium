// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_installer_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher_impl.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager_mock.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_task.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "components/prefs/pref_service.h"
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
  BorealisInstallerViewBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kBorealis, ash::features::kBorealisPermitted}, {});
  }

  // Disallow copy and assign.
  BorealisInstallerViewBrowserTest(const BorealisInstallerViewBrowserTest&) =
      delete;
  BorealisInstallerViewBrowserTest& operator=(
      const BorealisInstallerViewBrowserTest&) = delete;

  // DialogBrowserTest:
  void SetUpOnMainThread() override {
    app_launcher_ =
        std::make_unique<BorealisAppLauncherImpl>(browser()->profile());
    features_ = std::make_unique<BorealisFeatures>(browser()->profile());

    BorealisServiceFake* fake_service =
        BorealisServiceFake::UseFakeForTesting(browser()->profile());
    fake_service->SetContextManagerForTesting(&mock_context_manager_);
    fake_service->SetInstallerForTesting(&mock_installer_);
    fake_service->SetAppLauncherForTesting(app_launcher_.get());
    fake_service->SetFeaturesForTesting(features_.get());
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
    EXPECT_EQ(
        view_->GetPrimaryMessage(),
        l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_CONFIRMATION_TITLE));
    EXPECT_EQ(view_->GetProgressMessage(), u"");
  }

  void ExpectInstallationInProgress() {
    EXPECT_FALSE(HasAcceptButton());
    EXPECT_TRUE(HasCancelButton());
    EXPECT_EQ(view_->GetPrimaryMessage(),
              l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ONGOING_TITLE));
    EXPECT_EQ(
        view_->GetSecondaryMessage(),
        l10n_util::GetStringUTF16(IDS_BOREALIS_INSTALLER_ONGOING_MESSAGE));
    EXPECT_NE(view_->GetProgressMessage(), u"");
  }

  void ExpectInstallationFailed() {
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
    EXPECT_EQ(view_->GetProgressMessage(), u"");
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
    view_->CancelDialog();
    EXPECT_TRUE(view_->GetWidget()->IsClosed());
  }

  base::test::ScopedFeatureList feature_list_;
  ::testing::NiceMock<BorealisInstallerMock> mock_installer_;
  ::testing::NiceMock<BorealisContextManagerMock> mock_context_manager_;
  std::unique_ptr<BorealisAppLauncher> app_launcher_;
  std::unique_ptr<BorealisFeatures> features_;
  BorealisInstallerView* view_;
};

// Test that the dialog can be launched.
IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest, InvokeUi_default) {
  EXPECT_CALL(mock_installer_, Cancel());
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest, SucessfulInstall) {
  ShowUi("default");
  AcceptInstallation();

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kBorealisInstalledOnDevice, true);
  view_->OnInstallationEnded(InstallationResult::kSuccess, "");
  ExpectInstallationCompletedSucessfully();

  EXPECT_CALL(mock_context_manager_, IsRunning())
      .WillOnce(testing::Return(true));
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

  view_->OnInstallationEnded(error_type, "in progress");
  ExpectInstallationFailed();

  AcceptInstallation();

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kBorealisInstalledOnDevice, true);
  view_->OnInstallationEnded(InstallationResult::kSuccess, "");
  ExpectInstallationCompletedSucessfully();

  EXPECT_CALL(mock_context_manager_, IsRunning())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_context_manager_, StartBorealis(_));
  EXPECT_CALL(mock_installer_, RemoveObserver(_));
  view_->AcceptDialog();

  EXPECT_TRUE(view_->GetWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(BorealisInstallerViewBrowserTest,
                       ErrorDuringInstallation) {
  InstallationResult error_type =
      InstallationResult::kBorealisInstallInProgress;
  ShowUi("default");
  AcceptInstallation();

  view_->OnInstallationEnded(error_type, "in progress");
  ExpectInstallationFailed();

  ClickCancel();
}
}  // namespace
}  // namespace borealis
