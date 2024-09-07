// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_recovery_view.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_dialogue_browser_test_util.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/dialog_button.mojom.h"

constexpr crostini::CrostiniUISurface kUiSurface =
    crostini::CrostiniUISurface::kAppList;
constexpr char kDesktopFileId[] = "test_app";
constexpr int kDisplayId = 0;

namespace {

void ExpectFailure(const std::string& expected_failure_reason,
                   bool success,
                   const std::string& failure_reason) {
  EXPECT_FALSE(success);
  EXPECT_EQ(expected_failure_reason, failure_reason);
}
}  // namespace

class CrostiniRecoveryViewBrowserTest : public CrostiniDialogBrowserTest {
 public:
  CrostiniRecoveryViewBrowserTest()
      : CrostiniDialogBrowserTest(true /*register_termina*/),
        app_id_(crostini::CrostiniTestHelper::GenerateAppId(kDesktopFileId)) {}

  CrostiniRecoveryViewBrowserTest(const CrostiniRecoveryViewBrowserTest&) =
      delete;
  CrostiniRecoveryViewBrowserTest& operator=(
      const CrostiniRecoveryViewBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    CrostiniDialogBrowserTest::SetUpOnMainThread();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowCrostiniRecoveryView(browser()->profile(), kUiSurface, app_id(),
                             kDisplayId, {}, base::DoNothing());
  }

  void SetUncleanStartup() {
    auto* crostini_manager =
        crostini::CrostiniManager::GetForProfile(browser()->profile());
    crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName);
    crostini_manager->SetUncleanStartupForTesting(true);
  }

  CrostiniRecoveryView* ActiveView() {
    return CrostiniRecoveryView::GetActiveViewForTesting();
  }

  void WaitForViewDestroyed() {
    base::RunLoop().RunUntilIdle();
    ExpectNoView();
  }

  void ExpectView() {
    // A new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_TRUE(VerifyUi());
    // There is one view, and it's ours.
    EXPECT_NE(nullptr, ActiveView());
    EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk) |
                  static_cast<int>(ui::mojom::DialogButton::kCancel),
              ActiveView()->buttons());

    EXPECT_NE(ActiveView()->GetOkButton(), nullptr);
    EXPECT_NE(ActiveView()->GetCancelButton(), nullptr);
    EXPECT_TRUE(
        ActiveView()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
    EXPECT_TRUE(
        ActiveView()->IsDialogButtonEnabled(ui::mojom::DialogButton::kCancel));
  }

  void ExpectNoView() {
    // No new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_FALSE(VerifyUi());
    // Our view has really been deleted.
    EXPECT_EQ(nullptr, ActiveView());
  }

  bool IsUncleanStartup() {
    return crostini::CrostiniManager::GetForProfile(browser()->profile())
        ->IsUncleanStartup();
  }

  void RegisterApp() {
    vm_tools::apps::ApplicationList app_list =
        crostini::CrostiniTestHelper::BasicAppList(
            kDesktopFileId, crostini::kCrostiniDefaultVmName,
            crostini::kCrostiniDefaultContainerName);
    guest_os::GuestOsRegistryServiceFactory::GetForProfile(browser()->profile())
        ->UpdateApplicationList(app_list);
  }

  const std::string& app_id() const { return app_id_; }

 private:
  std::string app_id_;
};

// Test the dialog is actually launched.
IN_PROC_BROWSER_TEST_F(CrostiniRecoveryViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CrostiniRecoveryViewBrowserTest, NoViewOnNormalStartup) {
  base::HistogramTester histogram_tester;
  RegisterApp();

  crostini::LaunchCrostiniApp(browser()->profile(), app_id(), kDisplayId);
  ExpectNoView();

  histogram_tester.ExpectUniqueSample(
      "Crostini.RecoverySource",
      static_cast<base::HistogramBase::Sample>(kUiSurface), 0);
}

IN_PROC_BROWSER_TEST_F(CrostiniRecoveryViewBrowserTest, Cancel) {
  base::HistogramTester histogram_tester;

  SetUncleanStartup();
  RegisterApp();
  // Ensure Terminal System App is installed.
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();

  // First app should fail with 'cancelled for recovery'.
  crostini::LaunchCrostiniApp(
      browser()->profile(), app_id(), kDisplayId, {},
      base::BindOnce(&ExpectFailure, "cancelled for recovery"));
  ExpectView();

  // Apps launched while dialog is shown should fail with 'recovery in
  // progress'.
  crostini::LaunchCrostiniApp(
      browser()->profile(), app_id(), kDisplayId, {},
      base::BindOnce(&ExpectFailure, "recovery in progress"));

  // Click 'Cancel'.
  ActiveView()->CancelDialog();
  WaitForViewDestroyed();

  // Terminal should launch after use clicks 'Cancel'.
  Browser* terminal_browser = ash::FindSystemWebAppBrowser(
      browser()->profile(), ash::SystemWebAppType::TERMINAL);
  EXPECT_NE(nullptr, terminal_browser);

  // Any new apps launched should show the dialog again.
  crostini::LaunchCrostiniApp(
      browser()->profile(), app_id(), kDisplayId, {},
      base::BindOnce(&ExpectFailure, "cancelled for recovery"));
  ExpectView();

  ActiveView()->CancelDialog();
  WaitForViewDestroyed();

  EXPECT_TRUE(IsUncleanStartup());

  histogram_tester.ExpectUniqueSample(
      "Crostini.RecoverySource",
      static_cast<base::HistogramBase::Sample>(kUiSurface), 3);
}

IN_PROC_BROWSER_TEST_F(CrostiniRecoveryViewBrowserTest, Accept) {
  base::HistogramTester histogram_tester;

  SetUncleanStartup();
  RegisterApp();

  crostini::LaunchCrostiniApp(browser()->profile(), app_id(), kDisplayId);
  ExpectView();

  // Apps launched while dialog is shown should fail with 'recovery in
  // progress'.
  crostini::LaunchCrostiniApp(
      browser()->profile(), app_id(), kDisplayId, {},
      base::BindOnce(&ExpectFailure, "recovery in progress"));

  // Click 'Accept'.
  ActiveView()->AcceptDialog();

  // Buttons should be disabled after clicking Accept.
  EXPECT_FALSE(
      ActiveView()->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_FALSE(
      ActiveView()->IsDialogButtonEnabled(ui::mojom::DialogButton::kCancel));

  WaitForViewDestroyed();

  EXPECT_FALSE(IsUncleanStartup());

  // Apps now launch successfully.
  crostini::LaunchCrostiniApp(browser()->profile(), app_id(), kDisplayId);
  ExpectNoView();

  histogram_tester.ExpectUniqueSample(
      "Crostini.RecoverySource",
      static_cast<base::HistogramBase::Sample>(kUiSurface), 2);
}
