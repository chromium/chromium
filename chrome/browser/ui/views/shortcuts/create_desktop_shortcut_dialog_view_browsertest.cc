// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/picture_in_picture/document_picture_in_picture_mixin_test_base.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/shortcuts/create_shortcut_for_current_web_contents_task.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/shortcuts/create_desktop_shortcut.h"
#include "chrome/browser/ui/views/shortcuts/create_desktop_shortcut_delegate.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

namespace shortcuts {

namespace {

constexpr char kPageWithIcons[] = "/shortcuts/page_icons.html";

class CreateDesktopShortcutDialogViewBrowserTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest overrides:
  void ShowUi(const std::string& name) override {
    ShowDialogInBrowser(browser(), name);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 protected:
  void OverrideShortcutShownCallback(CreateShortcutDialogCallback callback) {
    shortcut_callback_ = std::move(callback);
  }

  void ShowDialogInBrowser(Browser* browser, const std::string& name) {
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser, GURL("https://example.com")));

    std::u16string title = base::UTF8ToUTF16(name);
    ShowCreateDesktopShortcutDialogForTesting(
        browser->tab_strip_model()->GetActiveWebContents(), gfx::ImageSkia(),
        title, std::move(shortcut_callback_));
  }

 private:
  CreateShortcutDialogCallback shortcut_callback_ = base::DoNothing();
};

IN_PROC_BROWSER_TEST_F(CreateDesktopShortcutDialogViewBrowserTest,
                       InvokeUiBasic) {
  base::UserActionTester action_tester;
  ShowAndVerifyUi();
  EXPECT_EQ(1,
            action_tester.GetActionCount("CreateDesktopShortcutDialogShown"));
}

// Dialog destruction due to navigations or other reasons are measured as
// cancellations from an user action perspective.
IN_PROC_BROWSER_TEST_F(CreateDesktopShortcutDialogViewBrowserTest,
                       InvokeUi_WidgetDestroyedOnNavigation) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");
  ShowUi("test_title");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL(url::kAboutBlankURL), /*number_of_navigations=*/1);

  destroy_waiter.Wait();
  EXPECT_EQ(
      1, action_tester.GetActionCount("CreateDesktopShortcutDialogCancelled"));
}

IN_PROC_BROWSER_TEST_F(CreateDesktopShortcutDialogViewBrowserTest,
                       InvokeUi_WidgetClosesOnVisibilityChange) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");
  ShowUi("test_title");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  // Navigate to a new tab.
  chrome::NewTab(browser());

  destroy_waiter.Wait();
  EXPECT_EQ(
      1, action_tester.GetActionCount("CreateDesktopShortcutDialogCancelled"));
}

IN_PROC_BROWSER_TEST_F(CreateDesktopShortcutDialogViewBrowserTest,
                       InvokeUi_WidgetClosesOnWebContentsDestruction) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");
  ShowUi("test_title");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->Close();

  destroy_waiter.Wait();
  EXPECT_EQ(
      1, action_tester.GetActionCount("CreateDesktopShortcutDialogCancelled"));
}

IN_PROC_BROWSER_TEST_F(CreateDesktopShortcutDialogViewBrowserTest,
                       InvokeUi_Accept) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");
  ShowUi("test_title");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::AcceptDialog(widget);
  destroy_waiter.Wait();
  EXPECT_EQ(
      1, action_tester.GetActionCount("CreateDesktopShortcutDialogAccepted"));
}

IN_PROC_BROWSER_TEST_F(CreateDesktopShortcutDialogViewBrowserTest,
                       InvokeUi_Cancel_TitleFilled) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");
  ShowUi("test_title");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::CancelDialog(widget);
  destroy_waiter.Wait();
  EXPECT_EQ(
      1, action_tester.GetActionCount("CreateDesktopShortcutDialogCancelled"));
}

IN_PROC_BROWSER_TEST_F(CreateDesktopShortcutDialogViewBrowserTest,
                       InvokeUi_Cancel_TitleEmpty) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");
  ShowUi(base::EmptyString());
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::CancelDialog(widget);
  destroy_waiter.Wait();
  EXPECT_EQ(
      1, action_tester.GetActionCount("CreateDesktopShortcutDialogCancelled"));
}

IN_PROC_BROWSER_TEST_F(CreateDesktopShortcutDialogViewBrowserTest,
                       InvokeUi_TitleHasNoProfileIfSingleProfile) {
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");

  base::test::TestFuture<std::optional<std::u16string>> test_future;
  OverrideShortcutShownCallback(test_future.GetCallback());

  ShowUi("ABC");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::AcceptDialog(widget);
  destroy_waiter.Wait();
  EXPECT_TRUE(test_future.Wait());

  std::optional<std::u16string> dialog_result = test_future.Get();
  EXPECT_TRUE(dialog_result.has_value());
  EXPECT_EQ(dialog_result.value(), u"ABC");
}

IN_PROC_BROWSER_TEST_F(CreateDesktopShortcutDialogViewBrowserTest,
                       InvokeUi_TitleHasProfileNameInfoMultiProfile) {
  // Create a new profile, and open a new browser window in that profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  Profile* new_profile =
      &profiles::testing::CreateProfileSync(profile_manager, new_path);

  base::test::TestFuture<Browser*> browser_future;
  // `is_new_profile` has to be set to false so that the profile picker is not
  // triggered.
  profiles::OpenBrowserWindowForProfile(
      browser_future.GetCallback(), /*always_create=*/true,
      /*is_new_profile=*/false, /*unblock_extensions=*/false, new_profile);
  EXPECT_TRUE(browser_future.Wait());
  Browser* new_browser = browser_future.Get();
  EXPECT_EQ(new_browser->profile(), new_profile);

  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");

  base::test::TestFuture<std::optional<std::u16string>> test_future;
  OverrideShortcutShownCallback(test_future.GetCallback());

  ShowDialogInBrowser(new_browser, "ABC");
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::AcceptDialog(widget);
  destroy_waiter.Wait();
  EXPECT_TRUE(test_future.Wait());

  std::optional<std::u16string> dialog_result = test_future.Get();
  EXPECT_TRUE(dialog_result.has_value());
  EXPECT_EQ(dialog_result.value(), u"ABC (Person 2)");
}

IN_PROC_BROWSER_TEST_F(CreateDesktopShortcutDialogViewBrowserTest,
                       DontShowMultipleDialogsIfAlreadyShown) {
  base::UserActionTester action_tester;
  std::u16string titles[] = {u"title1", u"title2"};
  base::test::TestFuture<std::optional<std::u16string>> test_future1,
      test_future2;

  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");
  ShowCreateDesktopShortcutDialogForTesting(
      browser()->tab_strip_model()->GetActiveWebContents(), gfx::ImageSkia(),
      titles[0], test_future1.GetCallback());
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();

  // Verify that a second request fails before the first dialog is closed.
  ShowCreateDesktopShortcutDialogForTesting(
      browser()->tab_strip_model()->GetActiveWebContents(), gfx::ImageSkia(),
      titles[1], test_future2.GetCallback());
  EXPECT_TRUE(test_future2.Wait());
  auto dialog_result2 = test_future2.Get();
  EXPECT_FALSE(dialog_result2.has_value());
  EXPECT_FALSE(test_future1.IsReady());
  EXPECT_FALSE(widget->IsClosed());

  // The original dialog can still be accepted.
  views::test::AcceptDialog(widget);
  EXPECT_TRUE(test_future1.Wait());
  auto dialog_result1 = test_future1.Get();
  EXPECT_TRUE(dialog_result1.has_value());
  EXPECT_EQ(dialog_result1.value(), titles[0]);
  EXPECT_EQ(
      1, action_tester.GetActionCount("CreateDesktopShortcutDialogAccepted"));

  // The second dialog wasn't shown, so it does not record being cancelled.
  EXPECT_EQ(
      0, action_tester.GetActionCount("CreateDesktopShortcutDialogCancelled"));
}

IN_PROC_BROWSER_TEST_F(CreateDesktopShortcutDialogViewBrowserTest,
                       CancelFromCreateShortcutDialogStopsShortcutCreation) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(kPageWithIcons)));
  base::HistogramTester histogram_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");

  base::test::TestFuture<bool> final_callback;
  CreateShortcutForWebContents(
      browser()->tab_strip_model()->GetActiveWebContents(),
      final_callback.GetCallback());
  views::Widget* widget = widget_waiter.WaitIfNeededAndGet();
  ASSERT_NE(widget, nullptr);

  views::test::WidgetDestroyedWaiter destroy_waiter(widget);
  views::test::CancelDialog(widget);
  destroy_waiter.Wait();
  ASSERT_TRUE(final_callback.Wait());
  EXPECT_FALSE(final_callback.Get<bool>());

  histogram_tester.ExpectBucketCount(
      "Shortcuts.CreationTask.Result",
      shortcuts::ShortcutCreationTaskResult::
          kUserCancelledShortcutCreationFromDialog,
      1);
}

class PictureInPictureCreateShortcutDialogOcclusionTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  void ShowDialogUi() {
    ShowCreateDesktopShortcutDialogForTesting(
        browser()->tab_strip_model()->GetActiveWebContents(), gfx::ImageSkia(),
        u"DialogTitle", base::DoNothing());
  }
  DocumentPictureInPictureMixinTestBase picture_in_picture_test_base_{
      &mixin_host_};
};

IN_PROC_BROWSER_TEST_F(PictureInPictureCreateShortcutDialogOcclusionTest,
                       PipWindowCloses) {
  picture_in_picture_test_base_.NavigateToURLAndEnterPictureInPicture(
      browser());
  auto* pip_web_contents =
      picture_in_picture_test_base_.window_controller()->GetChildWebContents();
  ASSERT_NE(nullptr, pip_web_contents);
  picture_in_picture_test_base_.WaitForPageLoad(pip_web_contents);

  // Show dialog.
  base::UserActionTester action_tester;
  views::NamedWidgetShownWaiter widget_waiter(
      views::test::AnyWidgetTestPasskey{}, "CreateDesktopShortcutDialog");
  ShowDialogUi();
  views::Widget* dialog_widget = widget_waiter.WaitIfNeededAndGet();
  EXPECT_NE(nullptr, dialog_widget);

  // Occlude dialog with picture in picture web contents, verify window is
  // closed but dialog stays open.
  PictureInPictureWindowManager::GetInstance()
      ->GetOcclusionTracker()
      ->SetWidgetOcclusionStateForTesting(dialog_widget, /*occluded=*/true);
  EXPECT_TRUE(picture_in_picture_test_base_.AwaitPipWindowClosedSuccessfully());
  EXPECT_NE(nullptr, dialog_widget);
  EXPECT_TRUE(dialog_widget->IsVisible());
}

}  // namespace

}  // namespace shortcuts
