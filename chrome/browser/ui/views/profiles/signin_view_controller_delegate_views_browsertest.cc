// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/signin_view_controller_delegate_views.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

class SigninViewControllerDelegateViewsBrowserTest : public DialogBrowserTest {
 public:
  SigninViewControllerDelegateViewsBrowserTest() = default;
  ~SigninViewControllerDelegateViewsBrowserTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    CreateDelegate(/*show_immediately=*/true);
  }

  // The delegate deletes itself when closed.
  // If `show_immediately` is false, call ResizeNativeView() to show the dialog.
  SigninViewControllerDelegateViews* CreateDelegate(bool show_immediately) {
    return new SigninViewControllerDelegateViews(
        SigninViewControllerDelegateViews::CreateSyncConfirmationWebView(
            browser(), SyncConfirmationStyle::kDefaultModal,
            /*is_sync_promo=*/false),
        browser(), ui::mojom::ModalType::kWindow,
        /*wait_for_size=*/!show_immediately, /*should_show_close_button=*/false,
        /*animate_on_resize=*/true);
  }

  // Closes the dialog and checks that the web contents were not leaked.
  void CloseDialog(SigninViewControllerDelegateViews* delegate) {
    content::WebContentsDestroyedWatcher watcher(delegate->GetWebContents());
    delegate->CloseModalSignin();
    watcher.Wait();
    EXPECT_TRUE(watcher.IsDestroyed());
  }
};

IN_PROC_BROWSER_TEST_F(SigninViewControllerDelegateViewsBrowserTest,
                       InvokeUi_SyncConfirmation) {
  ShowAndVerifyUi();
}

// Opens and closes the dialog immediately. This is a basic test at the
// `SigninViewController` API level, but does not test all the edge cases.
// Regression test for https://crbug.com/40191339.
IN_PROC_BROWSER_TEST_F(SigninViewControllerDelegateViewsBrowserTest,
                       CloseImmediately) {
  SigninViewController* controller = SigninViewController::From(browser());
  controller->ShowModalSyncConfirmationDialog(
      /*is_signin_intercept=*/false, /*is_sync_promo=*/false);
  content::WebContentsDestroyedWatcher watcher(
      controller->GetModalDialogWebContentsForTesting());
  // Close the dialog before it was displayed, this should not crash.
  controller->CloseModalSignin();
  // The web contents were destroyed.
  EXPECT_TRUE(watcher.IsDestroyed());
}

// Similar to SigninViewControllerDelegateViewsBrowserTest.CloseImmediately but
// specifically tests the case where the dialog is not shown at creation.
// Regression test for https://crbug.com/40191339.
IN_PROC_BROWSER_TEST_F(SigninViewControllerDelegateViewsBrowserTest,
                       CloseBeforeDisplay) {
  SigninViewControllerDelegateViews* delegate =
      CreateDelegate(/*show_immediately=*/false);
  // The dialog is not shown.
  EXPECT_FALSE(delegate->GetWidget());
  CloseDialog(delegate);
}

// Similar to SigninViewControllerDelegateViewsBrowserTest.CloseImmediately but
// specifically tests the case where the dialog is shown at creation.
// Regression test for https://crbug.com/40191339.
IN_PROC_BROWSER_TEST_F(SigninViewControllerDelegateViewsBrowserTest,
                       CloseAfterDisplay) {
  SigninViewControllerDelegateViews* delegate =
      CreateDelegate(/*show_immediately=*/true);
  // The dialog is shown.
  EXPECT_TRUE(delegate->GetWidget());
  CloseDialog(delegate);
}

// Creates a dialog that is not shown until the size is set. Checks that the
// dialog is initially shown with correct size.
// TODO(crbug.com/40214711): Fix unexpected dialog height on mac10.12.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ResizeBeforeDisplay DISABLED_ResizeBeforeDisplay
#else
#define MAYBE_ResizeBeforeDisplay ResizeBeforeDisplay
#endif
IN_PROC_BROWSER_TEST_F(SigninViewControllerDelegateViewsBrowserTest,
                       MAYBE_ResizeBeforeDisplay) {
  const int kDialogHeight = 255;
  SigninViewControllerDelegateViews* delegate =
      CreateDelegate(/*show_immediately=*/false);
  EXPECT_FALSE(delegate->GetWidget());
  delegate->ResizeNativeView(kDialogHeight);
  EXPECT_TRUE(delegate->GetWidget());
  EXPECT_EQ(delegate->GetContentsBounds().height(), kDialogHeight);
}

// Creates a dialog that is shown immediately with a default size. Checks that
// ResizeNativeView() changes the dialog size correctly.
IN_PROC_BROWSER_TEST_F(SigninViewControllerDelegateViewsBrowserTest,
                       ResizeAfterDisplay) {
  const int kDialogHeight = 255;
  SigninViewControllerDelegateViews* delegate =
      CreateDelegate(/*show_immediately=*/true);
  EXPECT_TRUE(delegate->GetWidget());

  // Disable resize animation.
  auto animation_mode_reset = gfx::AnimationTestApi::SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
  delegate->ResizeNativeView(kDialogHeight);
  EXPECT_TRUE(delegate->GetWidget());
  EXPECT_EQ(delegate->GetContentsBounds().height(), kDialogHeight);
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerDelegateViewsBrowserTest,
                       ModalDialogManagerWiredOnCreation) {
  SigninViewControllerDelegateViews* delegate =
      CreateDelegate(/*show_immediately=*/true);
  content::WebContents* web_contents = delegate->GetWebContents();
  ASSERT_NE(web_contents, nullptr);
  EXPECT_EQ(web_contents->GetDelegate(), delegate);

  auto* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  ASSERT_NE(manager, nullptr);
  EXPECT_EQ(manager->delegate(), delegate);

  CloseDialog(delegate);
}

IN_PROC_BROWSER_TEST_F(SigninViewControllerDelegateViewsBrowserTest,
                       ModalDialogManagerWiredOnSetWebContentsAndTeardown) {
  SigninViewControllerDelegateViews* delegate =
      CreateDelegate(/*show_immediately=*/true);

  std::unique_ptr<content::WebContents> initial_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->GetProfile()));
  delegate->SetWebContents(initial_web_contents.get());

  EXPECT_EQ(initial_web_contents->GetDelegate(), delegate);
  auto* initial_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(
          initial_web_contents.get());
  ASSERT_NE(initial_manager, nullptr);
  EXPECT_EQ(initial_manager->delegate(), delegate);

  std::unique_ptr<content::WebContents> new_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->GetProfile()));
  delegate->SetWebContents(new_web_contents.get());

  EXPECT_EQ(initial_web_contents->GetDelegate(), nullptr);
  EXPECT_EQ(initial_manager->delegate(), nullptr);
  EXPECT_EQ(new_web_contents->GetDelegate(), delegate);
  auto* new_manager = web_modal::WebContentsModalDialogManager::FromWebContents(
      new_web_contents.get());
  ASSERT_NE(new_manager, nullptr);
  EXPECT_EQ(new_manager->delegate(), delegate);

  views::test::WidgetDestroyedWaiter waiter(delegate->GetWidget());
  delegate->CloseModalSignin();
  waiter.Wait();

  EXPECT_EQ(new_web_contents->GetDelegate(), nullptr);
  EXPECT_EQ(new_manager->delegate(), nullptr);
}
