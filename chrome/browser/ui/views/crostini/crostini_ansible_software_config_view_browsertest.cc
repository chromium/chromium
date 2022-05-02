// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_ansible_software_config_view.h"

#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_dialogue_browser_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

class CrostiniAnsibleSoftwareConfigViewBrowserTest
    : public CrostiniDialogBrowserTest,
      public crostini::AnsibleManagementService::Observer {
 public:
  CrostiniAnsibleSoftwareConfigViewBrowserTest()
      : CrostiniDialogBrowserTest(true /*register_termina*/),
        container_id_(crostini::kCrostiniDefaultVmName,
                      crostini::kCrostiniDefaultContainerName),
        network_connection_tracker_(
            network::TestNetworkConnectionTracker::CreateInstance()) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kCrostiniAnsibleInfrastructure);
  }

  // CrostiniDialogBrowserTest:
  void ShowUi(const std::string& name) override {
    crostini::ShowCrostiniAnsibleSoftwareConfigView(browser()->profile());
  }

  CrostiniAnsibleSoftwareConfigView* ActiveView() {
    return CrostiniAnsibleSoftwareConfigView::GetActiveViewForTesting();
  }

  // crostini::AnsibleManagementService::Observer
  void OnAnsibleSoftwareConfigurationStarted(
      const crostini::ContainerId& container_id) override {}
  void OnAnsibleSoftwareConfigurationFinished(
      const crostini::ContainerId& container_id,
      bool success) override {}

  void OnApplyAnsiblePlaybook(
      const crostini::ContainerId& container_id) override {
    if (is_apply_ansible_success_) {
      EXPECT_NE(nullptr, ActiveView());
      vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal signal;
      signal.set_status(
          vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::SUCCEEDED);
      signal.set_vm_name(crostini::ContainerId::GetDefault().vm_name);
      signal.set_container_name(
          crostini::ContainerId::GetDefault().container_name);
      ansible_management_service()->OnApplyAnsiblePlaybookProgress(signal);
    } else {
      EXPECT_NE(nullptr, ActiveView());

      vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal signal;
      signal.set_status(
          vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::FAILED);
      signal.set_vm_name(crostini::ContainerId::GetDefault().vm_name);
      signal.set_container_name(
          crostini::ContainerId::GetDefault().container_name);
      signal.set_failure_details("apple");
      ansible_management_service()->OnApplyAnsiblePlaybookProgress(signal);
    }
  }
  void OnAnsibleSoftwareInstall(
      const crostini::ContainerId& container_id) override {
    if (is_install_ansible_success_) {
      EXPECT_NE(nullptr, ActiveView());
      EXPECT_TRUE(IsDefaultDialog());
      ansible_management_service()->OnInstallLinuxPackageProgress(
          container_id_, crostini::InstallLinuxPackageProgressStatus::SUCCEEDED,
          100,
          /*error_message=*/{});
    } else {
      EXPECT_NE(nullptr, ActiveView());
      EXPECT_TRUE(IsDefaultDialog());

      ansible_management_service()->OnInstallLinuxPackageProgress(
          container_id_, crostini::InstallLinuxPackageProgressStatus::FAILED, 0,
          /*error_message=*/{});
    }
  }

 protected:
  void SetUpOnMainThread() override {
    // NetworkConnectionTracker should be reset first.
    content::SetNetworkConnectionTrackerForTesting(nullptr);
    content::SetNetworkConnectionTrackerForTesting(
        network_connection_tracker_.get());

    test_helper_ = std::make_unique<crostini::AnsibleManagementTestHelper>(
        browser()->profile());
    test_helper_->SetUpAnsiblePlaybookPreference();
    run_loop_ = std::make_unique<base::RunLoop>();
    ansible_management_service()->AddObserver(this);

    // Set sensible defaults.
    is_install_ansible_success_ = true;
    is_apply_ansible_success_ = true;
  }

  void TearDownOnMainThread() override {
    ansible_management_service()->RemoveObserver(this);
  }

  void SetConnectionType(network::mojom::ConnectionType type) {
    network_connection_tracker_->SetConnectionType(type);
  }

  // A new Widget was created in ShowUi() or since the last VerifyUi().
  bool HasView() { return VerifyUi() && ActiveView(); }

  // No new Widget was created in ShowUi() or since last VerifyUi().
  bool HasNoView() {
    base::RunLoop().RunUntilIdle();
    return !VerifyUi() && ActiveView() == nullptr;
  }

  bool IsDefaultDialog() {
    return !HasAcceptButton() && !HasCancelButton() && HasDefaultStrings();
  }

  bool IsErrorDialog() {
    return HasAcceptButton() && !HasCancelButton() && HasErrorStrings();
  }

  bool IsErrorOfflineDialog() {
    return HasAcceptButton() && HasCancelButton() && HasErrorOfflineStrings();
  }

  base::RunLoop* run_loop() { return run_loop_.get(); }

  crostini::AnsibleManagementService* ansible_management_service() {
    return crostini::AnsibleManagementService::GetForProfile(
        browser()->profile());
  }

  void SetApplyAnsibleStatus(bool success) {
    is_apply_ansible_success_ = success;
  }

  void SetInstallAnsibleStatus(bool success) {
    is_install_ansible_success_ = success;
  }

  crostini::ContainerId container_id_;

 private:
  bool HasAcceptButton() { return ActiveView()->GetOkButton() != nullptr; }
  bool HasCancelButton() { return ActiveView()->GetCancelButton() != nullptr; }

  bool HasDefaultStrings() {
    return ActiveView()->GetWindowTitle() ==
               l10n_util::GetStringUTF16(
                   IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_LABEL) &&
           ActiveView()->GetSubtextLabelStringForTesting() ==
               l10n_util::GetStringUTF16(
                   IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_SUBTEXT);
  }

  bool HasErrorStrings() {
    return ActiveView()->GetWindowTitle() ==
               l10n_util::GetStringUTF16(
                   IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_LABEL) &&
           ActiveView()->GetSubtextLabelStringForTesting() ==
               l10n_util::GetStringUTF16(
                   IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_SUBTEXT);
  }

  bool HasErrorOfflineStrings() {
    return ActiveView()->GetWindowTitle() ==
               l10n_util::GetStringFUTF16(
                   IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_OFFLINE_LABEL,
                   ui::GetChromeOSDeviceName()) &&
           ActiveView()->GetSubtextLabelStringForTesting() ==
               l10n_util::GetStringUTF16(
                   IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_OFFLINE_SUBTEXT);
  }

  bool is_install_ansible_success_;
  bool is_apply_ansible_success_;
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  std::unique_ptr<crostini::AnsibleManagementTestHelper> test_helper_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::test::ScopedFeatureList scoped_feature_list_;
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

  ActiveView()->OnAnsibleSoftwareConfigurationFinished(container_id_, true);

  EXPECT_TRUE(HasNoView());
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       UnsuccessfulFlow) {
  ShowUi("default");

  EXPECT_TRUE(HasView());
  EXPECT_TRUE(IsDefaultDialog());

  ActiveView()->OnAnsibleSoftwareConfigurationFinished(container_id_, false);

  EXPECT_NE(nullptr, ActiveView());
  EXPECT_TRUE(IsErrorDialog());
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       UnsuccessfulFlow_Offline) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);

  ShowUi("default");

  EXPECT_TRUE(HasView());
  EXPECT_TRUE(IsDefaultDialog());

  ActiveView()->OnAnsibleSoftwareConfigurationFinished(container_id_, false);

  EXPECT_NE(nullptr, ActiveView());
  EXPECT_TRUE(IsErrorOfflineDialog());
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       UnsuccessfulFlow_Offline_CanRetry) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);

  ShowUi("default");

  EXPECT_TRUE(HasView());
  EXPECT_TRUE(IsDefaultDialog());

  ActiveView()->OnAnsibleSoftwareConfigurationFinished(container_id_, false);

  EXPECT_NE(nullptr, ActiveView());
  EXPECT_TRUE(IsErrorOfflineDialog());

  // Retry button clicked.
  ActiveView()->AcceptDialog();

  EXPECT_NE(nullptr, ActiveView());
  EXPECT_TRUE(IsDefaultDialog());
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       UnsuccessfulFlow_Offline_Cancel) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);

  ShowUi("default");

  EXPECT_TRUE(HasView());
  EXPECT_TRUE(IsDefaultDialog());

  ActiveView()->OnAnsibleSoftwareConfigurationFinished(container_id_, false);

  EXPECT_NE(nullptr, ActiveView());
  EXPECT_TRUE(IsErrorOfflineDialog());

  // Cancel button clicked.
  ActiveView()->CancelDialog();

  EXPECT_TRUE(HasNoView());
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       AnsibleConfigFlow_Successful) {
  ansible_management_service()->ConfigureContainer(
      crostini::ContainerId::GetDefault(),
      browser()->profile()->GetPrefs()->GetFilePath(
          crostini::prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindLambdaForTesting([&](bool success) { run_loop()->Quit(); }));

  run_loop()->Run();

  EXPECT_TRUE(HasNoView());
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       AnsibleConfigFlow_InstallationFailed) {
  // Set install failure. No need to set apply because it should never reach
  // there.
  SetInstallAnsibleStatus(false);
  ansible_management_service()->ConfigureContainer(
      crostini::ContainerId::GetDefault(),
      browser()->profile()->GetPrefs()->GetFilePath(
          crostini::prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindLambdaForTesting([&](bool success) { run_loop()->Quit(); }));

  run_loop()->Run();

  EXPECT_NE(nullptr, ActiveView());
  EXPECT_TRUE(IsErrorDialog());
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       AnsibleConfigFlow_ApplicationFailed) {
  // Set apply failure
  SetApplyAnsibleStatus(false);
  ansible_management_service()->ConfigureContainer(
      crostini::ContainerId::GetDefault(),
      browser()->profile()->GetPrefs()->GetFilePath(
          crostini::prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindLambdaForTesting(
          [&](bool success) { std::move(run_loop()->QuitClosure()).Run(); }));

  run_loop()->Run();

  EXPECT_NE(nullptr, ActiveView());
  EXPECT_TRUE(IsErrorDialog());
}
