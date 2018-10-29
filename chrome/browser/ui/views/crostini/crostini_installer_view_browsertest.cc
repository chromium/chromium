// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_installer_view.h"

#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_model_builder.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_browser_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/dbus/fake_cros_disks_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/crx_file/id_util.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/window/dialog_client_view.h"

class CrostiniInstallerViewBrowserTest : public CrostiniDialogBrowserTest {
 public:
  class WaitingFakeConciergeClient : public chromeos::FakeConciergeClient {
   public:
    void StartTerminaVm(
        const vm_tools::concierge::StartVmRequest& request,
        chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
            callback) override {
      chromeos::FakeConciergeClient::StartTerminaVm(request,
                                                    std::move(callback));
      if (closure_) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                      std::move(closure_));
      }
    }

    void WaitForStartTerminaVmCalled() {
      base::RunLoop loop;
      closure_ = loop.QuitClosure();
      loop.Run();
      EXPECT_TRUE(start_termina_vm_called());
    }

   private:
    base::OnceClosure closure_;
  };

  class WaitingDiskMountManagerObserver
      : public chromeos::disks::DiskMountManager::Observer {
   public:
    void OnMountEvent(chromeos::disks::DiskMountManager::MountEvent event,
                      chromeos::MountError error_code,
                      const chromeos::disks::DiskMountManager::MountPointInfo&
                          mount_info) override {
      run_loop_->Quit();
    }

    void WaitForMountEvent() {
      chromeos::disks::DiskMountManager::GetInstance()->AddObserver(this);
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }

   private:
    std::unique_ptr<base::RunLoop> run_loop_;
  };

  CrostiniInstallerViewBrowserTest()
      : CrostiniDialogBrowserTest(true /*register_termina*/),
        waiting_fake_concierge_client_(new WaitingFakeConciergeClient()),
        waiting_disk_mount_manager_observer_(
            new WaitingDiskMountManagerObserver) {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetConciergeClient(
        base::WrapUnique(waiting_fake_concierge_client_));
    static_cast<chromeos::FakeCrosDisksClient*>(
        chromeos::DBusThreadManager::Get()->GetCrosDisksClient())
        ->AddCustomMountPointCallback(base::BindRepeating(
            &CrostiniInstallerViewBrowserTest::MaybeMountCrostini,
            base::Unretained(this)));
  }

  // CrostiniDialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowCrostiniInstallerView(browser()->profile(),
                              crostini::CrostiniUISurface::kSettings);
  }

  void SetUpOnMainThread() override {
    CrostiniDialogBrowserTest::SetUpOnMainThread();
  }

  CrostiniInstallerView* ActiveView() {
    return CrostiniInstallerView::GetActiveViewForTesting();
  }

  bool HasAcceptButton() {
    return ActiveView()->GetDialogClientView()->ok_button() != nullptr;
  }

  bool HasCancelButton() {
    return ActiveView()->GetDialogClientView()->cancel_button() != nullptr;
  }

 protected:
  // Owned by chromeos::DBusThreadManager
  WaitingFakeConciergeClient* waiting_fake_concierge_client_ = nullptr;
  WaitingDiskMountManagerObserver* waiting_disk_mount_manager_observer_ =
      nullptr;

 private:
  base::FilePath MaybeMountCrostini(
      const std::string& source_path,
      const std::vector<std::string>& mount_options) {
    GURL source_url(source_path);
    DCHECK(source_url.is_valid());
    if (source_url.scheme() != "sshfs") {
      return {};
    }
    EXPECT_EQ("sshfs://stub-user@hostname:", source_path);
    return base::FilePath(
        browser()->profile()->GetPath().Append("crostini_test"));
  }
  DISALLOW_COPY_AND_ASSIGN(CrostiniInstallerViewBrowserTest);
};

// Test the dialog is actually launched from the app launcher.
IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, InstallFlow) {
  base::HistogramTester histogram_tester;

  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());
  EXPECT_EQ(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
            ActiveView()->GetDialogButtons());

  EXPECT_TRUE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());

  ActiveView()->GetDialogClientView()->AcceptWindow();
  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed());
  EXPECT_FALSE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());

  waiting_disk_mount_manager_observer_->WaitForMountEvent();

  // RunUntilIdle in this case will run the rest of the install steps including
  // launching the terminal, on the UI thread.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, ActiveView());

  histogram_tester.ExpectUniqueSample(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstallerView::SetupResult::kSuccess),
      1);
}

IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, InstallFlow_Offline) {
  base::HistogramTester histogram_tester;
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());
  EXPECT_EQ(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
            ActiveView()->GetDialogButtons());

  EXPECT_TRUE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());

  ActiveView()->GetDialogClientView()->AcceptWindow();
  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed());
  EXPECT_TRUE(HasAcceptButton());
  EXPECT_EQ(ActiveView()->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
            l10n_util::GetStringUTF16(IDS_CROSTINI_INSTALLER_RETRY_BUTTON));
  EXPECT_TRUE(HasCancelButton());

  ActiveView()->GetDialogClientView()->CancelWindow();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, ActiveView());

  histogram_tester.ExpectUniqueSample(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstallerView::SetupResult::kErrorOffline),
      1);
}

IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, Cancel) {
  base::HistogramTester histogram_tester;

  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());
  ActiveView()->GetDialogClientView()->CancelWindow();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, ActiveView());

  histogram_tester.ExpectUniqueSample(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstallerView::SetupResult::kNotStarted),
      1);
}

IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, ErrorThenCancel) {
  base::HistogramTester histogram_tester;
  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());
  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VM_STATUS_FAILURE);
  waiting_fake_concierge_client_->set_start_vm_response(std::move(response));

  ActiveView()->GetDialogClientView()->AcceptWindow();
  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed());
  waiting_fake_concierge_client_->WaitForStartTerminaVmCalled();
  ActiveView()->GetDialogClientView()->CancelWindow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, ActiveView());

  histogram_tester.ExpectUniqueSample(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstallerView::SetupResult::kErrorStartingTermina),
      1);
}
