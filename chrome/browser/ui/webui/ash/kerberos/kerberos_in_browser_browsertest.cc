// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/kerberos/kerberos_in_browser_dialog.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {
constexpr test::UIPath kCancelButtonPath = {"redirect-dialog", "cancel-button"};
constexpr test::UIPath kOpenSettingsButtonPath = {"redirect-dialog",
                                                  "settings-button"};

bool IsSettingsWindowOpened() {
  auto* browser_list = BrowserList::GetInstance();
  return base::ranges::count_if(*browser_list, [](Browser* browser) {
           return ash::IsBrowserForSystemWebApp(
               browser, ash::SystemWebAppType::SETTINGS);
         }) != 0;
}
}  // namespace

class KerberosInBrowserDialogButtonTest : public InProcessBrowserTest {
 public:
  KerberosInBrowserDialogButtonTest() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kKerberosInBrowserRedirect);
  }

  KerberosInBrowserDialogButtonTest(const KerberosInBrowserDialogButtonTest&) =
      delete;
  KerberosInBrowserDialogButtonTest& operator=(
      const KerberosInBrowserDialogButtonTest&) = delete;

  ~KerberosInBrowserDialogButtonTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();
  }

  void PressButton(const test::UIPath& button_path) {
    auto* frame = webui_->GetRenderFrameHost();
    ASSERT_TRUE(frame);

    // Waiting for the DOM to be fully loaded.
    std::make_unique<test::TestPredicateWaiter>(
        base::BindRepeating(&content::RenderFrameHost::IsDOMContentLoaded,
                            base::Unretained(frame)))
        ->Wait();

    test::JSChecker checker = test::JSChecker(frame);
    checker.CreateVisibilityWaiter(/*visibility=*/true, button_path)->Wait();
    checker.ExpectValidPath(button_path);
    checker.ClickOnPath(button_path);
  }

  void EnsureWebUIAvailable() {
    auto* dialog = ash::KerberosInBrowserDialog::GetDialogForTesting();
    ASSERT_TRUE(dialog);
    webui_ = dialog->GetWebUIForTest();
    ASSERT_TRUE(webui_);
  }

  void WaitUntilDialogIsClosed() {
    std::make_unique<test::TestPredicateWaiter>(base::BindRepeating([]() {
      return !KerberosInBrowserDialog::IsShown();
    }))->Wait();
  }

  raw_ptr<content::WebUI, DanglingUntriaged> webui_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(KerberosInBrowserDialogButtonTest, CancelButton) {
  ash::KerberosInBrowserDialog::Show();
  EXPECT_TRUE(ash::KerberosInBrowserDialog::IsShown());
  EXPECT_FALSE(IsSettingsWindowOpened());

  EnsureWebUIAvailable();
  PressButton(kCancelButtonPath);
  WaitUntilDialogIsClosed();

  // Pressing the cancel button should not open a new settings window.
  EXPECT_FALSE(IsSettingsWindowOpened());
}

IN_PROC_BROWSER_TEST_F(KerberosInBrowserDialogButtonTest, SettingsButton) {
  ash::KerberosInBrowserDialog::Show();
  EXPECT_TRUE(ash::KerberosInBrowserDialog::IsShown());
  EXPECT_FALSE(IsSettingsWindowOpened());

  EnsureWebUIAvailable();
  PressButton(kOpenSettingsButtonPath);
  WaitUntilDialogIsClosed();

  // Waiting for a new OS settings window to be opened.
  std::make_unique<test::TestPredicateWaiter>(base::BindRepeating([]() {
    return IsSettingsWindowOpened();
  }))->Wait();
}

class KerberosInBrowserDialogFeatureDisabledTest
    : public KerberosInBrowserDialogButtonTest {
 public:
  KerberosInBrowserDialogFeatureDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        net::features::kKerberosInBrowserRedirect);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(KerberosInBrowserDialogFeatureDisabledTest, Smoke) {
  ash::KerberosInBrowserDialog::Show();

  // If the feature is disabled the system dialog is created anyway, but the
  // WebUI is not loaded.
  EXPECT_TRUE(ash::KerberosInBrowserDialog::IsShown());
  auto* dialog = ash::KerberosInBrowserDialog::GetDialogForTesting();
  ASSERT_TRUE(dialog);
  webui_ = dialog->GetWebUIForTest();
  ASSERT_FALSE(webui_);
}

}  // namespace ash
