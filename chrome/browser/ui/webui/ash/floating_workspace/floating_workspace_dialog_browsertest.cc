// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_dialog.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {
// There are 3 different screens, but cancel button exists in every one
// of them and the path is the same.
constexpr test::UIPath kCancelButtonPath = {"floating-workspace-dialog",
                                            "cancelButton"};

}  // namespace

class FloatingWorkspaceDialogTest : public InProcessBrowserTest {
 public:
  FloatingWorkspaceDialogTest() {}

  FloatingWorkspaceDialogTest(const FloatingWorkspaceDialogTest&) = delete;
  FloatingWorkspaceDialogTest& operator=(const FloatingWorkspaceDialogTest&) =
      delete;

  ~FloatingWorkspaceDialogTest() override = default;

 protected:
  void PressCancelButton() {
    auto* frame = webui_->GetRenderFrameHost();
    ASSERT_TRUE(frame);

    // Waiting for the DOM to be fully loaded.
    std::make_unique<test::TestPredicateWaiter>(
        base::BindRepeating(&content::RenderFrameHost::IsDOMContentLoaded,
                            base::Unretained(frame)))
        ->Wait();

    test::JSChecker checker = test::JSChecker(frame);
    checker.ExpectValidPath(kCancelButtonPath);
    checker.CreateVisibilityWaiter(/*visibility=*/true, kCancelButtonPath)
        ->Wait();
    checker.ClickOnPath(kCancelButtonPath);
  }

  void EnsureWebUIAvailable() {
    auto* dialog = SystemWebDialogDelegate::FindInstance(
        GURL{chrome::kChromeUIFloatingWorkspaceDialogURL}.spec());
    ASSERT_TRUE(dialog);
    webui_ = dialog->GetWebUIForTest();
    ASSERT_TRUE(webui_);
  }

  void WaitUntilDialogIsClosed() {
    std::make_unique<test::TestPredicateWaiter>(base::BindRepeating([]() {
      return !FloatingWorkspaceDialog::IsShown();
    }))->Wait();
  }

  raw_ptr<content::WebUI, DisableDanglingPtrDetection> webui_;
};

IN_PROC_BROWSER_TEST_F(FloatingWorkspaceDialogTest, DefaultDialog) {
  ASSERT_FALSE(ash::FloatingWorkspaceDialog::IsShown());
  ash::FloatingWorkspaceDialog::ShowDefaultScreen();
  EXPECT_EQ(ash::FloatingWorkspaceDialog::State::kDefault,
            ash::FloatingWorkspaceDialog::IsShown());

  EnsureWebUIAvailable();
  PressCancelButton();
  WaitUntilDialogIsClosed();
  EXPECT_FALSE(ash::FloatingWorkspaceDialog::IsShown());
}

IN_PROC_BROWSER_TEST_F(FloatingWorkspaceDialogTest, NetworkDialog) {
  ASSERT_FALSE(ash::FloatingWorkspaceDialog::IsShown());
  ash::FloatingWorkspaceDialog::ShowNetworkScreen();
  EXPECT_EQ(ash::FloatingWorkspaceDialog::State::kNetwork,
            ash::FloatingWorkspaceDialog::IsShown());

  EnsureWebUIAvailable();
  PressCancelButton();
  WaitUntilDialogIsClosed();
  EXPECT_FALSE(ash::FloatingWorkspaceDialog::IsShown());
}

IN_PROC_BROWSER_TEST_F(FloatingWorkspaceDialogTest, ErrorDialog) {
  ASSERT_FALSE(ash::FloatingWorkspaceDialog::IsShown());
  ash::FloatingWorkspaceDialog::ShowErrorScreen();
  EXPECT_EQ(ash::FloatingWorkspaceDialog::State::kError,
            ash::FloatingWorkspaceDialog::IsShown());

  EnsureWebUIAvailable();
  PressCancelButton();
  WaitUntilDialogIsClosed();
  EXPECT_FALSE(ash::FloatingWorkspaceDialog::IsShown());
}

IN_PROC_BROWSER_TEST_F(FloatingWorkspaceDialogTest, ScreenChange) {
  ASSERT_FALSE(ash::FloatingWorkspaceDialog::IsShown());
  ash::FloatingWorkspaceDialog::ShowDefaultScreen();
  EXPECT_EQ(ash::FloatingWorkspaceDialog::State::kDefault,
            ash::FloatingWorkspaceDialog::IsShown());

  ash::FloatingWorkspaceDialog::ShowErrorScreen();
  EXPECT_EQ(ash::FloatingWorkspaceDialog::State::kError,
            ash::FloatingWorkspaceDialog::IsShown());

  ash::FloatingWorkspaceDialog::ShowNetworkScreen();
  EXPECT_EQ(ash::FloatingWorkspaceDialog::State::kNetwork,
            ash::FloatingWorkspaceDialog::IsShown());

  // Calling the screen twice, since this behaviour should be also supported.
  ash::FloatingWorkspaceDialog::ShowDefaultScreen();
  EXPECT_EQ(ash::FloatingWorkspaceDialog::State::kDefault,
            ash::FloatingWorkspaceDialog::IsShown());
  ash::FloatingWorkspaceDialog::ShowDefaultScreen();
  EXPECT_EQ(ash::FloatingWorkspaceDialog::State::kDefault,
            ash::FloatingWorkspaceDialog::IsShown());
}

}  // namespace ash
