// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_uninstaller_view.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_dialogue_browser_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/dialog_button.mojom.h"

class CrostiniUninstallerViewBrowserTest : public CrostiniDialogBrowserTest {
 public:
  class WaitingFakeConciergeClient : public ash::FakeConciergeClient {
   public:
    explicit WaitingFakeConciergeClient(ash::FakeCiceroneClient* client)
        : ash::FakeConciergeClient(client) {}

    void StopVm(
        const vm_tools::concierge::StopVmRequest& request,
        chromeos::DBusMethodCallback<vm_tools::concierge::StopVmResponse>
            callback) override {
      ash::FakeConciergeClient::StopVm(request, std::move(callback));
      if (closure_) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(closure_));
      }
    }

    void WaitForStopVmCalled() {
      base::RunLoop loop;
      closure_ = loop.QuitClosure();
      loop.Run();
      EXPECT_GE(stop_vm_call_count(), 1);
    }

   private:
    base::OnceClosure closure_;
  };

  CrostiniUninstallerViewBrowserTest()
      : CrostiniUninstallerViewBrowserTest(true /*register_termina*/) {}

  explicit CrostiniUninstallerViewBrowserTest(bool register_termina)
      : CrostiniDialogBrowserTest(register_termina) {
    ash::ConciergeClient::Shutdown();
    // After the browser's mainloop is terminated. chromeos::ShutdownDBus() will
    // delete the object.
    waiting_fake_concierge_client_ = new WaitingFakeConciergeClient(nullptr);
  }

  CrostiniUninstallerViewBrowserTest(
      const CrostiniUninstallerViewBrowserTest&) = delete;
  CrostiniUninstallerViewBrowserTest& operator=(
      const CrostiniUninstallerViewBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    crostini::ShowCrostiniUninstallerView(browser()->profile());
  }

  CrostiniUninstallerView* ActiveView() {
    return CrostiniUninstallerView::GetActiveViewForTesting();
  }

  bool HasAcceptButton() { return ActiveView()->GetOkButton() != nullptr; }

  bool HasCancelButton() { return ActiveView()->GetCancelButton() != nullptr; }

  void WaitForViewDestroyed() {
    base::RunLoop run_loop;
    ActiveView()->set_destructor_callback_for_testing(run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_EQ(nullptr, ActiveView());
  }

 protected:
  raw_ptr<WaitingFakeConciergeClient, DanglingUntriaged>
      waiting_fake_concierge_client_;
};

class CrostiniUninstalledUninstallerViewBrowserTest
    : public CrostiniUninstallerViewBrowserTest {
 public:
  CrostiniUninstalledUninstallerViewBrowserTest()
      : CrostiniUninstallerViewBrowserTest(false /*register_termina*/) {}

  CrostiniUninstalledUninstallerViewBrowserTest(
      const CrostiniUninstalledUninstallerViewBrowserTest&) = delete;
  CrostiniUninstalledUninstallerViewBrowserTest& operator=(
      const CrostiniUninstalledUninstallerViewBrowserTest&) = delete;
};

// Test the dialog is actually launched from the app launcher.
IN_PROC_BROWSER_TEST_F(CrostiniUninstallerViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CrostiniUninstallerViewBrowserTest, UninstallFlow) {
  base::HistogramTester histogram_tester;

  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());
  EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel),
            ActiveView()->buttons());

  EXPECT_TRUE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());

  ActiveView()->AcceptDialog();
  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed());
  EXPECT_FALSE(HasAcceptButton());
  EXPECT_FALSE(HasCancelButton());

  WaitForViewDestroyed();

  histogram_tester.ExpectUniqueSample(
      "Crostini.UninstallResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniUninstallerView::UninstallResult::kSuccess),
      1);
}

IN_PROC_BROWSER_TEST_F(CrostiniUninstalledUninstallerViewBrowserTest,
                       OfflineUninstallFlowWithoutTermina) {
  base::HistogramTester histogram_tester;

  SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);

  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());
  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed());
  EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel),
            ActiveView()->buttons());

  EXPECT_TRUE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());

  ActiveView()->AcceptDialog();

  WaitForViewDestroyed();

  histogram_tester.ExpectUniqueSample(
      "Crostini.UninstallResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniUninstallerView::UninstallResult::kSuccess),
      1);
}

IN_PROC_BROWSER_TEST_F(CrostiniUninstallerViewBrowserTest, Cancel) {
  base::HistogramTester histogram_tester;

  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());
  ActiveView()->CancelDialog();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());
  WaitForViewDestroyed();

  histogram_tester.ExpectUniqueSample(
      "Crostini.UninstallResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniUninstallerView::UninstallResult::kCancelled),
      1);
}

IN_PROC_BROWSER_TEST_F(CrostiniUninstallerViewBrowserTest, ErrorThenCancel) {
  base::HistogramTester histogram_tester;
  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());
  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  waiting_fake_concierge_client_->set_stop_vm_response(std::move(response));

  ActiveView()->AcceptDialog();
  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed());
  waiting_fake_concierge_client_->WaitForStopVmCalled();
  EXPECT_TRUE(HasCancelButton());
  ActiveView()->CancelDialog();
  WaitForViewDestroyed();

  histogram_tester.ExpectUniqueSample(
      "Crostini.UninstallResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniUninstallerView::UninstallResult::kError),
      1);
}
