// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader_dialog.h"

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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_dialogue_browser_test_util.h"
#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kDesktopFileId[] = "test_app";
constexpr int kDisplayId = 0;

class CrostiniUpgraderDialogBrowserTest : public CrostiniDialogBrowserTest {
 public:
  CrostiniUpgraderDialogBrowserTest()
      : CrostiniDialogBrowserTest(true /*register_termina*/),
        app_id_(crostini::CrostiniTestHelper::GenerateAppId(kDesktopFileId)) {}

  CrostiniUpgraderDialogBrowserTest(const CrostiniUpgraderDialogBrowserTest&) =
      delete;
  CrostiniUpgraderDialogBrowserTest& operator=(
      const CrostiniUpgraderDialogBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ash::CrostiniUpgraderDialog::Show(browser()->profile(), base::DoNothing(),
                                      false);
  }

  ash::CrostiniUpgraderDialog* GetCrostiniUpgraderDialog() {
    auto url = GURL{chrome::kChromeUICrostiniUpgraderUrl};
    return static_cast<ash::CrostiniUpgraderDialog*>(
        ash::SystemWebDialogDelegate::FindInstance(url.spec()));
  }

  void SafelyCloseDialog() {
    auto* upgrader_dialog = GetCrostiniUpgraderDialog();
    // Make sure the WebUI has launches sufficiently. Closing immediately would
    // miss breakages in the underlying plumbing.
    auto* web_contents = upgrader_dialog->GetWebUIForTest()->GetWebContents();
    WaitForLoadFinished(web_contents);

    // Now there should be enough WebUI hooked up to close properly.
    base::RunLoop run_loop;
    upgrader_dialog->SetDeletionClosureForTesting(run_loop.QuitClosure());
    upgrader_dialog->Close();
    run_loop.Run();
  }

  void ExpectDialog() {
    // A new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_TRUE(crostini_manager()->GetCrostiniDialogStatus(
        crostini::DialogType::UPGRADER));

    EXPECT_NE(nullptr, GetCrostiniUpgraderDialog());
  }

  void ExpectNoDialog() {
    // No new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_FALSE(crostini_manager()->GetCrostiniDialogStatus(
        crostini::DialogType::UPGRADER));
    // Our dialog has really been deleted.
    EXPECT_EQ(nullptr, GetCrostiniUpgraderDialog());
  }

  void RegisterApp() {
    vm_tools::apps::ApplicationList app_list =
        crostini::CrostiniTestHelper::BasicAppList(
            kDesktopFileId, crostini::kCrostiniDefaultVmName,
            crostini::kCrostiniDefaultContainerName);
    guest_os::GuestOsRegistryServiceFactory::GetForProfile(browser()->profile())
        ->UpdateApplicationList(app_list);
  }

  void DowngradeOSRelease() {
    vm_tools::cicerone::OsRelease os_release;
    os_release.set_id("debian");
    os_release.set_version_id("9");
    auto container_id = crostini::DefaultContainerId();
    crostini_manager()->SetContainerOsRelease(container_id, os_release);
  }

  const std::string& app_id() const { return app_id_; }

  crostini::CrostiniManager* crostini_manager() {
    return crostini::CrostiniManager::GetForProfile(browser()->profile());
  }

 private:
  std::string app_id_;
};

IN_PROC_BROWSER_TEST_F(CrostiniUpgraderDialogBrowserTest,
                       NoDialogBeforeLaunch) {
  base::HistogramTester histogram_tester;
  RegisterApp();

  bool is_successful_app_launch = false;
  ExpectNoDialog();
  base::RunLoop run_loop;
  crostini::LaunchCrostiniApp(
      browser()->profile(), app_id(), kDisplayId, {},
      base::BindOnce(
          [](base::RunLoop* run_loop, bool* is_successful_app_launch,
             bool success, const std::string& failure_reason) {
            EXPECT_TRUE(success) << failure_reason;
            run_loop->Quit();
          },
          &run_loop, &is_successful_app_launch));
  run_loop.Run();

  ExpectNoDialog();
}

IN_PROC_BROWSER_TEST_F(CrostiniUpgraderDialogBrowserTest, ShowsOnAppLaunch) {
  base::HistogramTester histogram_tester;

  DowngradeOSRelease();
  RegisterApp();

  ash::CrostiniUpgraderDialog::Show(browser()->profile(), base::DoNothing());
  ExpectDialog();

  base::RunLoop run_loop;
  bool is_successful_app_launch = false;
  crostini::LaunchCrostiniApp(
      browser()->profile(), app_id(), kDisplayId, {},
      base::BindOnce(
          [](base::RunLoop* run_loop, bool* is_successful_app_launch,
             bool success, const std::string& failure_reason) {
            *is_successful_app_launch = success;
            run_loop->Quit();
          },
          &run_loop, &is_successful_app_launch));

  run_loop.Run();
  ExpectDialog();
  EXPECT_FALSE(is_successful_app_launch);

  SafelyCloseDialog();
  ExpectNoDialog();

  histogram_tester.ExpectUniqueSample(
      "Crostini.UpgradeDialogEvent",
      static_cast<base::HistogramBase::Sample>(
          crostini::UpgradeDialogEvent::kDialogShown),
      1);
}
