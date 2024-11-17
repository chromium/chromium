// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_ansible_software_config_view.h"

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service_factory.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
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

constexpr char kProgressString[] = "Yesh milord. More work?";

class CrostiniAnsibleSoftwareConfigViewBrowserTest
    : public CrostiniDialogBrowserTest,
      public crostini::AnsibleManagementService::Observer {
 public:
  CrostiniAnsibleSoftwareConfigViewBrowserTest()
      : CrostiniDialogBrowserTest(true /*register_termina*/),
        network_connection_tracker_(
            network::TestNetworkConnectionTracker::CreateInstance()) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kCrostiniAnsibleInfrastructure);
  }

  // CrostiniDialogBrowserTest:
  void ShowUi(const std::string& name) override {
    views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
        std::make_unique<CrostiniAnsibleSoftwareConfigView>(
            browser()->profile(), crostini::DefaultContainerId()),
        nullptr, nullptr);
    ansible_management_service()->AddConfigurationTaskForTesting(
        crostini::DefaultContainerId(), widget);
    widget->Show();
  }

  CrostiniAnsibleSoftwareConfigView* ActiveView(
      const guest_os::GuestId& container_id) {
    if (ansible_management_service()->GetDialogWidgetForTesting(container_id)) {
      return (CrostiniAnsibleSoftwareConfigView*)ansible_management_service()
          ->GetDialogWidgetForTesting(container_id)
          ->widget_delegate();
    } else {
      return nullptr;
    }
  }

  // crostini::AnsibleManagementService::Observer
  void OnAnsibleSoftwareConfigurationStarted(
      const guest_os::GuestId& container_id) override {}

  void OnAnsibleSoftwareConfigurationProgress(
      const guest_os::GuestId& container_id,
      const std::vector<std::string>& status_lines) override {}

  void OnAnsibleSoftwareConfigurationFinished(
      const guest_os::GuestId& container_id,
      bool success) override {}

  void OnAnsibleSoftwareConfigurationUiPrompt(
      const guest_os::GuestId& container_id,
      bool interactive) override {
    if (interactive && accept_) {
      ActiveView(container_id)->Accept();
    } else if (!accept_) {
      run_loop()->Quit();
    }
  }

  void OnApplyAnsiblePlaybook(const guest_os::GuestId& container_id) override {
    if (send_ansible_progress_) {
      EXPECT_NE(nullptr, ActiveView(container_id));
      vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal signal;
      signal.set_status(
          vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::IN_PROGRESS);
      signal.set_vm_name(container_id.vm_name);
      signal.set_container_name(container_id.container_name);
      signal.add_status_string(kProgressString);
      ansible_management_service()->OnApplyAnsiblePlaybookProgress(signal);
      status_string_ =
          ActiveView(container_id)->GetProgressLabelStringForTesting();
    }
    if (is_apply_ansible_success_) {
      EXPECT_NE(nullptr, ActiveView(container_id));
      vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal signal;
      signal.set_status(
          vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::SUCCEEDED);
      signal.set_vm_name(container_id.vm_name);
      signal.set_container_name(container_id.container_name);
      ansible_management_service()->OnApplyAnsiblePlaybookProgress(signal);
    } else {
      EXPECT_NE(nullptr, ActiveView(container_id));

      vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal signal;
      signal.set_status(
          vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::FAILED);
      signal.set_vm_name(container_id.vm_name);
      signal.set_container_name(container_id.container_name);
      signal.set_failure_details("apple");
      ansible_management_service()->OnApplyAnsiblePlaybookProgress(signal);
    }
  }
  void OnAnsibleSoftwareInstall(
      const guest_os::GuestId& container_id) override {
    if (is_install_ansible_success_) {
      EXPECT_NE(nullptr, ActiveView(container_id));
      EXPECT_TRUE(IsDefaultDialog(container_id));
      ansible_management_service()->OnInstallLinuxPackageProgress(
          container_id, crostini::InstallLinuxPackageProgressStatus::SUCCEEDED,
          100,
          /*error_message=*/{});
    } else {
      EXPECT_NE(nullptr, ActiveView(container_id));
      EXPECT_TRUE(IsDefaultDialog(container_id));

      ansible_management_service()->OnInstallLinuxPackageProgress(
          container_id, crostini::InstallLinuxPackageProgressStatus::FAILED, 0,
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
    accept_ = true;
    is_install_ansible_success_ = true;
    is_apply_ansible_success_ = true;
    send_ansible_progress_ = false;
  }

  void TearDownOnMainThread() override {
    ansible_management_service()->RemoveObserver(this);
  }

  void SetConnectionType(network::mojom::ConnectionType type) {
    network_connection_tracker_->SetConnectionType(type);
  }

  // A new Widget was created in ShowUi() or since the last VerifyUi().
  bool HasView(const guest_os::GuestId& container_id) {
    return VerifyUi() && ActiveView(container_id);
  }

  // No new Widget was created in ShowUi() or since last VerifyUi().
  bool HasNoView(const guest_os::GuestId& container_id) {
    base::RunLoop().RunUntilIdle();
    return !VerifyUi() && ActiveView(container_id) == nullptr;
  }

  bool IsDefaultDialog(const guest_os::GuestId& container_id) {
    return !HasAcceptButton(container_id) && HasCancelButton(container_id) &&
           HasDefaultStrings(container_id);
  }

  bool IsErrorDialog(const guest_os::GuestId& container_id) {
    return HasAcceptButton(container_id) && !HasCancelButton(container_id) &&
           HasErrorStrings(container_id);
  }

  bool IsErrorOfflineDialog(const guest_os::GuestId& container_id) {
    return HasAcceptButton(container_id) && HasCancelButton(container_id) &&
           HasErrorOfflineStrings(container_id);
  }

  base::RunLoop* run_loop() { return run_loop_.get(); }

  crostini::AnsibleManagementService* ansible_management_service() {
    return crostini::AnsibleManagementServiceFactory::GetForProfile(
        browser()->profile());
  }

  void SetApplyAnsibleStatus(bool success) {
    is_apply_ansible_success_ = success;
  }

  void SetInstallAnsibleStatus(bool success) {
    is_install_ansible_success_ = success;
  }

  void SetSendAnsibleProgress(bool show_progress) {
    send_ansible_progress_ = show_progress;
  }

  void SetAccept(bool accept) { accept_ = accept; }

  std::u16string status_string_;

 private:
  bool HasAcceptButton(const guest_os::GuestId& container_id) {
    return ActiveView(container_id)->GetOkButton() != nullptr;
  }
  bool HasCancelButton(const guest_os::GuestId& container_id) {
    return ActiveView(container_id)->GetCancelButton() != nullptr;
  }

  bool HasDefaultStrings(const guest_os::GuestId& container_id) {
    return ActiveView(container_id)->GetWindowTitle() ==
               l10n_util::GetStringUTF16(
                   IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_LABEL) +
                   u": " + base::UTF8ToUTF16(container_id.container_name) &&
           ActiveView(container_id)->GetSubtextLabelStringForTesting() ==
               l10n_util::GetStringUTF16(
                   IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_SUBTEXT);
  }

  bool HasErrorStrings(const guest_os::GuestId& container_id) {
    return ActiveView(container_id)->GetWindowTitle() ==
               l10n_util::GetStringUTF16(
                   IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_LABEL) +
                   u": " + base::UTF8ToUTF16(container_id.container_name) &&
           ActiveView(container_id)->GetSubtextLabelStringForTesting() ==
               l10n_util::GetStringUTF16(
                   IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_SUBTEXT);
  }

  bool HasErrorOfflineStrings(const guest_os::GuestId& container_id) {
    return ActiveView(container_id)->GetWindowTitle() ==
               l10n_util::GetStringFUTF16(
                   IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_OFFLINE_LABEL,
                   ui::GetChromeOSDeviceName()) +
                   u": " + base::UTF8ToUTF16(container_id.container_name) &&
           ActiveView(container_id)->GetSubtextLabelStringForTesting() ==
               l10n_util::GetStringUTF16(
                   IDS_CROSTINI_ANSIBLE_SOFTWARE_CONFIG_ERROR_OFFLINE_SUBTEXT);
  }

  bool accept_;
  bool is_install_ansible_success_;
  bool is_apply_ansible_success_;
  bool send_ansible_progress_;
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

  EXPECT_TRUE(HasView(crostini::DefaultContainerId()));
  EXPECT_TRUE(IsDefaultDialog(crostini::DefaultContainerId()));

  ActiveView(crostini::DefaultContainerId())
      ->OnAnsibleSoftwareConfigurationFinished(crostini::DefaultContainerId(),
                                               true);

  EXPECT_TRUE(HasNoView(crostini::DefaultContainerId()));
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       SuccessfulFlow_Cancel) {
  ShowUi("default");

  EXPECT_TRUE(HasView(crostini::DefaultContainerId()));
  EXPECT_TRUE(IsDefaultDialog(crostini::DefaultContainerId()));

  ActiveView(crostini::DefaultContainerId())->Cancel();

  EXPECT_TRUE(HasNoView(crostini::DefaultContainerId()));
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       UnsuccessfulFlow) {
  ShowUi("default");

  EXPECT_TRUE(HasView(crostini::DefaultContainerId()));
  EXPECT_TRUE(IsDefaultDialog(crostini::DefaultContainerId()));

  ActiveView(crostini::DefaultContainerId())
      ->OnAnsibleSoftwareConfigurationFinished(crostini::DefaultContainerId(),
                                               false);

  EXPECT_NE(nullptr, ActiveView(crostini::DefaultContainerId()));
  EXPECT_TRUE(IsErrorDialog(crostini::DefaultContainerId()));
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       UnsuccessfulFlow_Offline) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);

  ShowUi("default");

  EXPECT_TRUE(HasView(crostini::DefaultContainerId()));
  EXPECT_TRUE(IsDefaultDialog(crostini::DefaultContainerId()));

  ActiveView(crostini::DefaultContainerId())
      ->OnAnsibleSoftwareConfigurationFinished(crostini::DefaultContainerId(),
                                               false);

  EXPECT_NE(nullptr, ActiveView(crostini::DefaultContainerId()));
  EXPECT_TRUE(IsErrorOfflineDialog(crostini::DefaultContainerId()));
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       UnsuccessfulFlow_Offline_CanRetry) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);

  ShowUi("default");

  EXPECT_TRUE(HasView(crostini::DefaultContainerId()));
  EXPECT_TRUE(IsDefaultDialog(crostini::DefaultContainerId()));

  ActiveView(crostini::DefaultContainerId())
      ->OnAnsibleSoftwareConfigurationFinished(crostini::DefaultContainerId(),
                                               false);

  EXPECT_NE(nullptr, ActiveView(crostini::DefaultContainerId()));
  EXPECT_TRUE(IsErrorOfflineDialog(crostini::DefaultContainerId()));

  // Retry button clicked.
  ActiveView(crostini::DefaultContainerId())->AcceptDialog();

  EXPECT_NE(nullptr, ActiveView(crostini::DefaultContainerId()));
  EXPECT_TRUE(IsDefaultDialog(crostini::DefaultContainerId()));
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       UnsuccessfulFlow_Offline_Cancel) {
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);

  ShowUi("default");

  EXPECT_TRUE(HasView(crostini::DefaultContainerId()));
  EXPECT_TRUE(IsDefaultDialog(crostini::DefaultContainerId()));

  ActiveView(crostini::DefaultContainerId())
      ->OnAnsibleSoftwareConfigurationFinished(crostini::DefaultContainerId(),
                                               false);

  EXPECT_NE(nullptr, ActiveView(crostini::DefaultContainerId()));
  EXPECT_TRUE(IsErrorOfflineDialog(crostini::DefaultContainerId()));

  // Cancel button clicked.
  ActiveView(crostini::DefaultContainerId())->CancelDialog();

  EXPECT_TRUE(HasNoView(crostini::DefaultContainerId()));
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       AnsibleConfigFlow_Successful) {
  ansible_management_service()->ConfigureContainer(
      crostini::DefaultContainerId(),
      browser()->profile()->GetPrefs()->GetFilePath(
          crostini::prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindLambdaForTesting([&](bool success) { run_loop()->Quit(); }));

  run_loop()->Run();

  EXPECT_TRUE(HasNoView(crostini::DefaultContainerId()));
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       AnsibleConfigFlow_Multiple_Successful) {
  guest_os::GuestId container1(vm_tools::apps::VmType::TERMINA, "intersetingvm",
                               "ihaveseenbeyond");
  guest_os::GuestId container2(vm_tools::apps::VmType::TERMINA, "supersecretvm",
                               "nothingtoseehereofficer");
  bool done_once = false;
  ansible_management_service()->ConfigureContainer(
      container1,
      browser()->profile()->GetPrefs()->GetFilePath(
          crostini::prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindLambdaForTesting([&](bool success) {
        if (done_once)
          run_loop()->Quit();
        else
          done_once = true;
      }));

  ansible_management_service()->ConfigureContainer(
      container2,
      browser()->profile()->GetPrefs()->GetFilePath(
          crostini::prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindLambdaForTesting([&](bool success) {
        if (done_once)
          run_loop()->Quit();
        else
          done_once = true;
      }));
  run_loop()->Run();
  EXPECT_TRUE(HasNoView(container1));
  EXPECT_TRUE(HasNoView(container2));
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       AnsibleConfigFlowWithProgress_Successful) {
  SetSendAnsibleProgress(true);
  ansible_management_service()->ConfigureContainer(
      crostini::DefaultContainerId(),
      browser()->profile()->GetPrefs()->GetFilePath(
          crostini::prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindLambdaForTesting([&](bool success) { run_loop()->Quit(); }));

  run_loop()->Run();
  std::u16string expected;
  // -1 to anti the null at the end of the string.
  ASSERT_TRUE(base::UTF8ToUTF16(kProgressString, sizeof(kProgressString) - 1,
                                &expected));
  EXPECT_EQ(status_string_, expected);

  EXPECT_TRUE(HasNoView(crostini::DefaultContainerId()));
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       AnsibleConfigFlow_InstallationFailed) {
  // Set install failure. No need to set apply because it should never reach
  // there.
  SetAccept(false);
  SetInstallAnsibleStatus(false);
  ansible_management_service()->ConfigureContainer(
      crostini::DefaultContainerId(),
      browser()->profile()->GetPrefs()->GetFilePath(
          crostini::prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindLambdaForTesting([&](bool success) { run_loop()->Quit(); }));

  run_loop()->Run();

  EXPECT_NE(nullptr, ActiveView(crostini::DefaultContainerId()));
  EXPECT_TRUE(IsErrorDialog(crostini::DefaultContainerId()));
}

IN_PROC_BROWSER_TEST_F(CrostiniAnsibleSoftwareConfigViewBrowserTest,
                       AnsibleConfigFlow_ApplicationFailed) {
  // Set apply failure
  SetAccept(false);
  SetApplyAnsibleStatus(false);
  ansible_management_service()->ConfigureContainer(
      crostini::DefaultContainerId(),
      browser()->profile()->GetPrefs()->GetFilePath(
          crostini::prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindLambdaForTesting(
          [&](bool success) { std::move(run_loop()->QuitClosure()).Run(); }));

  run_loop()->Run();

  EXPECT_NE(nullptr, ActiveView(crostini::DefaultContainerId()));
  EXPECT_TRUE(IsErrorDialog(crostini::DefaultContainerId()));
}
