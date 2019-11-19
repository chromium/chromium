// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_ansible_software_config_view.h"

#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/crostini/ansible/ansible_management_service.h"
#include "chrome/browser/chromeos/crostini/ansible/ansible_management_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_browser_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/window/dialog_client_view.h"

class CrostiniAnsibleSoftwareConfigViewBrowserTest
    : public CrostiniDialogBrowserTest {
 public:
  CrostiniAnsibleSoftwareConfigViewBrowserTest()
      : CrostiniDialogBrowserTest(true /*register_termina*/),
        container_id_(crostini::kCrostiniDefaultVmName,
                      crostini::kCrostiniDefaultContainerName) {}

  // CrostiniDialogBrowserTest:
  void ShowUi(const std::string& name) override {
    crostini::ShowCrostiniAnsibleSoftwareConfigView(browser()->profile());
  }

  CrostiniAnsibleSoftwareConfigView* ActiveView() {
    return CrostiniAnsibleSoftwareConfigView::GetActiveViewForTesting();
  }

 protected:
  void SetUpOnMainThread() override {
    test_helper_ = std::make_unique<crostini::AnsibleManagementTestHelper>(
        browser()->profile());
    test_helper_->SetUpAnsiblePlaybookPreference();
  }

  // A new Widget was created in ShowUi() or since the last VerifyUi().
  bool HasView() { return VerifyUi() && ActiveView() != nullptr; }

  // No new Widget was created in ShowUi() or since last VerifyUi().
  bool HasNoView() {
    base::RunLoop().RunUntilIdle();
    return !VerifyUi() && ActiveView() == nullptr;
  }

  bool IsDefaultDialog() { return !HasAcceptButton() && HasDefaultStrings(); }

  bool IsErrorDialog() { return HasAcceptButton() && HasErrorStrings(); }

  crostini::AnsibleManagementService* ansible_management_service() {
    return crostini::AnsibleManagementService::GetForProfile(
        browser()->profile());
  }

  crostini::ContainerId container_id_;

 private:
  bool HasAcceptButton() {
    return ActiveView()->GetDialogClientView()->ok_button() != nullptr;
  }

  bool HasDefaultStrings() {
    return (ActiveView()->GetWindowTitle().compare(l10n_util::GetStringUTF16(
                IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_LABEL)) == 0) &&
           (ActiveView()->GetSubtextLabelStringForTesting().compare(
                l10n_util::GetStringUTF16(
                    IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_SUBTEXT)) == 0);
  }

  bool HasErrorStrings() {
    return (ActiveView()->GetWindowTitle().compare(l10n_util::GetStringUTF16(
                IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_LABEL)) == 0) &&
           (ActiveView()->GetSubtextLabelStringForTesting().compare(
                l10n_util::GetStringUTF16(
                    IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_SUBTEXT)) == 0);
  }

  std::unique_ptr<crostini::AnsibleManagementTestHelper> test_helper_;
};

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       SuccessfulFlow) {
  ShowUi("default");

  EXPECT_TRUE(HasView());
  EXPECT_TRUE(IsDefaultDialog());

  ActiveView()->OnAnsibleSoftwareConfigurationFinished(true);

  EXPECT_TRUE(HasNoView());
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       UnsuccessfulFlow) {
  ShowUi("default");

  EXPECT_TRUE(HasView());
  EXPECT_TRUE(IsDefaultDialog());

  ActiveView()->OnAnsibleSoftwareConfigurationFinished(false);

  EXPECT_NE(nullptr, ActiveView());
  EXPECT_TRUE(IsErrorDialog());
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       AnsibleConfigFlow_Successful) {
  ansible_management_service()->ConfigureDefaultContainer(base::DoNothing());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(HasView());
  EXPECT_TRUE(IsDefaultDialog());

  ansible_management_service()->OnInstallLinuxPackageProgress(
      container_id_, crostini::InstallLinuxPackageProgressStatus::SUCCEEDED,
      100);
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(nullptr, ActiveView());

  ansible_management_service()->OnApplyAnsiblePlaybookProgress(
      vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::SUCCEEDED);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(HasNoView());
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       AnsibleConfigFlow_InstallationFailed) {
  ansible_management_service()->ConfigureDefaultContainer(base::DoNothing());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(HasView());
  EXPECT_TRUE(IsDefaultDialog());

  ansible_management_service()->OnInstallLinuxPackageProgress(
      container_id_, crostini::InstallLinuxPackageProgressStatus::FAILED, 0);
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(nullptr, ActiveView());
  EXPECT_TRUE(IsErrorDialog());
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       AnsibleConfigFlow_ApplicationFailed) {
  ansible_management_service()->ConfigureDefaultContainer(base::DoNothing());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(HasView());
  EXPECT_TRUE(IsDefaultDialog());

  ansible_management_service()->OnInstallLinuxPackageProgress(
      container_id_, crostini::InstallLinuxPackageProgressStatus::SUCCEEDED,
      100);
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(nullptr, ActiveView());

  ansible_management_service()->OnApplyAnsiblePlaybookProgress(
      vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::FAILED);
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(nullptr, ActiveView());
  EXPECT_TRUE(IsErrorDialog());
}
