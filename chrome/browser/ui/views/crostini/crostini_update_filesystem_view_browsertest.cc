// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_update_filesystem_view.h"

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_dialogue_browser_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/dialog_button.mojom.h"

ash::FakeCiceroneClient* GetFakeCiceroneClient() {
  return ash::FakeCiceroneClient::Get();
}

class CrostiniUpdateFilesystemViewBrowserTest
    : public CrostiniDialogBrowserTest {
 public:
  CrostiniUpdateFilesystemViewBrowserTest()
      : CrostiniDialogBrowserTest(true /*register_termina*/) {}

  CrostiniUpdateFilesystemViewBrowserTest(
      const CrostiniUpdateFilesystemViewBrowserTest&) = delete;
  CrostiniUpdateFilesystemViewBrowserTest& operator=(
      const CrostiniUpdateFilesystemViewBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    PrepareShowCrostiniUpdateFilesystemView(
        browser()->profile(), crostini::CrostiniUISurface::kAppList);
    base::RunLoop().RunUntilIdle();
  }

  CrostiniUpdateFilesystemView* ActiveView() {
    return CrostiniUpdateFilesystemView::GetActiveViewForTesting();
  }

  bool HasAcceptButton() { return ActiveView()->GetOkButton() != nullptr; }

  bool HasCancelButton() { return ActiveView()->GetCancelButton() != nullptr; }

  void ExpectView() {
    base::RunLoop().RunUntilIdle();
    // A new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_TRUE(VerifyUi());
    // There is one view, and it's ours.
    EXPECT_NE(nullptr, ActiveView());
  }

  void ExpectNoView() {
    base::RunLoop().RunUntilIdle();
    // No new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_FALSE(VerifyUi());
    // Our view has really been deleted.
    EXPECT_EQ(nullptr, ActiveView());
  }

  const guest_os::GuestId kGuestId =
      guest_os::GuestId(crostini::kCrostiniDefaultVmType,
                        "vm_name",
                        "container_name");
};

// Test the dialog is actually launched.
IN_PROC_BROWSER_TEST_F(CrostiniUpdateFilesystemViewBrowserTest,
                       InvokeUi_default) {
  crostini::SetCrostiniUpdateFilesystemSkipDelayForTesting(true);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CrostiniUpdateFilesystemViewBrowserTest, HitOK) {
  base::HistogramTester histogram_tester;
  crostini::SetCrostiniUpdateFilesystemSkipDelayForTesting(true);

  ShowUi("default");
  ExpectView();
  EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk),
            ActiveView()->buttons());

  EXPECT_TRUE(HasAcceptButton());
  EXPECT_FALSE(HasCancelButton());

  ActiveView()->AcceptDialog();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());
  ExpectNoView();

  histogram_tester.ExpectUniqueSample(
      "Crostini.UpgradeContainerSource",
      static_cast<base::HistogramBase::Sample>(
          crostini::CrostiniUISurface::kAppList),
      1);
}

IN_PROC_BROWSER_TEST_F(CrostiniUpdateFilesystemViewBrowserTest,
                       StartLxdContainerNoUpgradeNeeded) {
  base::HistogramTester histogram_tester;
  crostini::SetCrostiniUpdateFilesystemSkipDelayForTesting(true);

  vm_tools::cicerone::StartLxdContainerResponse reply;
  reply.set_status(vm_tools::cicerone::StartLxdContainerResponse::STARTING);
  GetFakeCiceroneClient()->set_start_lxd_container_response(reply);

  crostini::CrostiniManager::GetForProfile(browser()->profile())
      ->StartLxdContainer(kGuestId, base::DoNothing());
  ExpectNoView();
}

IN_PROC_BROWSER_TEST_F(CrostiniUpdateFilesystemViewBrowserTest,
                       StartLxdContainerUpgradeNeeded) {
  base::HistogramTester histogram_tester;
  crostini::SetCrostiniUpdateFilesystemSkipDelayForTesting(true);

  vm_tools::cicerone::StartLxdContainerResponse reply;
  reply.set_status(vm_tools::cicerone::StartLxdContainerResponse::REMAPPING);
  GetFakeCiceroneClient()->set_start_lxd_container_response(reply);

  crostini::CrostiniManager::GetForProfile(browser()->profile())
      ->StartLxdContainer(kGuestId, base::DoNothing());
  ExpectView();

  ActiveView()->AcceptDialog();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());
  ExpectNoView();

  histogram_tester.ExpectUniqueSample(
      "Crostini.UpgradeContainerSource",
      static_cast<base::HistogramBase::Sample>(
          crostini::CrostiniUISurface::kAppList),
      1);
}
