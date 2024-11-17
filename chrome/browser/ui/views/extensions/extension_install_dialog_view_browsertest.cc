// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_install_dialog_view.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/message_formatter.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_install_prompt_test_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_icon_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using extensions::PermissionIDSet;
using extensions::PermissionMessage;
using extensions::PermissionMessages;
using extensions::PermissionSet;

namespace {

void CloseAndWait(views::Widget* widget) {
  views::test::WidgetDestroyedWaiter waiter(widget);
  widget->Close();
  waiter.Wait();
}

}  // namespace

class ExtensionInstallDialogViewTestBase
    : public extensions::ExtensionBrowserTest {
 public:
  ExtensionInstallDialogViewTestBase(
      const ExtensionInstallDialogViewTestBase&) = delete;
  ExtensionInstallDialogViewTestBase& operator=(
      const ExtensionInstallDialogViewTestBase&) = delete;

 protected:
  ExtensionInstallDialogViewTestBase();

  void SetUpOnMainThread() override;

  // Creates and returns an install prompt of |prompt_type|.
  std::unique_ptr<ExtensionInstallPrompt::Prompt> CreatePrompt(
      ExtensionInstallPrompt::PromptType prompt_type);
  std::unique_ptr<ExtensionInstallPrompt::Prompt> CreatePrompt(
      ExtensionInstallPrompt::PromptType prompt_type,
      const extensions::Extension* extension);

  content::WebContents* web_contents() { return web_contents_; }

 private:
  raw_ptr<const extensions::Extension, AcrossTasksDanglingUntriaged> extension_;
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_;
};

ExtensionInstallDialogViewTestBase::ExtensionInstallDialogViewTestBase()
    : extension_(nullptr), web_contents_(nullptr) {}

void ExtensionInstallDialogViewTestBase::SetUpOnMainThread() {
  extensions::ExtensionBrowserTest::SetUpOnMainThread();

  extension_ = LoadExtension(test_data_dir_.AppendASCII(
      "install_prompt/permissions_scrollbar_regression"));

  web_contents_ = browser()->tab_strip_model()->GetWebContentsAt(0);
}

std::unique_ptr<ExtensionInstallPrompt::Prompt>
ExtensionInstallDialogViewTestBase::CreatePrompt(
    ExtensionInstallPrompt::PromptType prompt_type) {
  return CreatePrompt(prompt_type, extension_);
}

std::unique_ptr<ExtensionInstallPrompt::Prompt>
ExtensionInstallDialogViewTestBase::CreatePrompt(
    ExtensionInstallPrompt::PromptType prompt_type,
    const extensions::Extension* extension) {
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt(
      new ExtensionInstallPrompt::Prompt(prompt_type));
  prompt->set_extension(extension);

  std::unique_ptr<extensions::ExtensionIconManager> icon_manager(
      new extensions::ExtensionIconManager());
  prompt->set_icon(icon_manager->GetIcon(extension->id()));

  return prompt;
}

class ScrollbarTest : public ExtensionInstallDialogViewTestBase {
 public:
  ScrollbarTest(const ScrollbarTest&) = delete;
  ScrollbarTest& operator=(const ScrollbarTest&) = delete;

 protected:
  ScrollbarTest() {}

  bool IsScrollbarVisible(
      std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt);
};

bool ScrollbarTest::IsScrollbarVisible(
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt) {
  ExtensionInstallDialogView* dialog = new ExtensionInstallDialogView(
      std::make_unique<ExtensionInstallPromptShowParams>(web_contents()),
      base::DoNothing(), std::move(prompt));

  // Create the modal view around the install dialog view.
  views::Widget* modal = constrained_window::CreateBrowserModalDialogViews(
      dialog, web_contents()->GetTopLevelNativeWindow());
  modal->Show();
  content::RunAllTasksUntilIdle();

  // Check if the vertical scrollbar is visible.
  return dialog->scroll_view()->vertical_scroll_bar()->GetVisible();
}

// Tests that a scrollbar _is_ shown for an excessively long extension
// install prompt.
IN_PROC_BROWSER_TEST_F(ScrollbarTest, LongPromptScrollbar) {
  std::u16string permission_string(u"Test");
  PermissionMessages permissions;
  for (int i = 0; i < 20; i++) {
    permissions.push_back(PermissionMessage(permission_string,
                                            PermissionIDSet()));
  }
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt =
      CreatePrompt(ExtensionInstallPrompt::PERMISSIONS_PROMPT);
  prompt->AddPermissionMessages(permissions);
  ASSERT_TRUE(IsScrollbarVisible(std::move(prompt)))
      << "Scrollbar is not visible";
}

// Tests that a scrollbar isn't shown for this regression case.
// See crbug.com/385570 for details.
// TODO(http://crbug.com/988934): Flaky on some Mac release bots.
#if BUILDFLAG(IS_MAC) && defined(NDEBUG)
#define MAYBE_ScrollbarRegression DISABLED_ScrollbarRegression
#else
#define MAYBE_ScrollbarRegression ScrollbarRegression
#endif
IN_PROC_BROWSER_TEST_F(ScrollbarTest, MAYBE_ScrollbarRegression) {
  std::u16string permission_string(
      u"Read and modify your data on *.facebook.com");
  PermissionMessages permissions;
  permissions.push_back(PermissionMessage(permission_string,
                                          PermissionIDSet()));
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt =
      CreatePrompt(ExtensionInstallPrompt::PERMISSIONS_PROMPT);
  prompt->AddPermissionMessages(permissions);
  ASSERT_FALSE(IsScrollbarVisible(std::move(prompt))) << "Scrollbar is visible";
}

class ExtensionInstallDialogViewTest
    : public ExtensionInstallDialogViewTestBase {
 public:
  ExtensionInstallDialogViewTest() = default;
  ExtensionInstallDialogViewTest(const ExtensionInstallDialogViewTest&) =
      delete;
  ExtensionInstallDialogViewTest& operator=(
      const ExtensionInstallDialogViewTest&) = delete;

 protected:
  ExtensionInstallDialogView* CreateAndShowPrompt(
      ExtensionInstallPromptTestHelper* helper) {
    auto dialog = std::make_unique<ExtensionInstallDialogView>(
        std::make_unique<ExtensionInstallPromptShowParams>(web_contents()),
        helper->GetCallback(),
        CreatePrompt(ExtensionInstallPrompt::INSTALL_PROMPT));
    ExtensionInstallDialogView* delegate_view = dialog.get();

    views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
        dialog.release(), nullptr,
        platform_util::GetViewForWindow(
            browser()->window()->GetNativeWindow()));
    modal_dialog->Show();

    return delegate_view;
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewTest, NotifyDelegate) {
  {
    // User presses install. Note that we have to wait for the 0ms delay for the
    // install button to become enabled, hence the RunLoop later.
    ExtensionInstallDialogView::SetInstallButtonDelayForTesting(0);
    ExtensionInstallPromptTestHelper helper;
    ExtensionInstallDialogView* delegate_view = CreateAndShowPrompt(&helper);
    base::RunLoop().RunUntilIdle();
    delegate_view->AcceptDialog();
    EXPECT_EQ(ExtensionInstallPrompt::Result::ACCEPTED, helper.result());
  }
  {
    // User presses cancel.
    ExtensionInstallPromptTestHelper helper;
    ExtensionInstallDialogView* delegate_view = CreateAndShowPrompt(&helper);
    delegate_view->CancelDialog();
    EXPECT_EQ(ExtensionInstallPrompt::Result::USER_CANCELED, helper.result());
  }
  {
    // Dialog is closed without the user explicitly choosing to proceed or
    // cancel.
    ExtensionInstallPromptTestHelper helper;
    ExtensionInstallDialogView* delegate_view = CreateAndShowPrompt(&helper);
    // Note that the close button isn't present, but the dialog can still be
    // closed this way via Esc.
    EXPECT_FALSE(delegate_view->ShouldShowCloseButton());
    CloseAndWait(delegate_view->GetWidget());
    // TODO(devlin): Should this be ABORTED?
    EXPECT_EQ(ExtensionInstallPrompt::Result::USER_CANCELED, helper.result());
  }
}

// Regression test for crbug.com/40059470. Tests that the originating web
// contents are activated and the installation prompt is shown there, if the
// user switches the tab after starting the installation.
IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewTest,
                       ActivateWebContentsOnTabChange) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* originating_web_contents =
      tab_strip_model->GetActiveWebContents();

  AddBlankTabAndShow(browser());
  ASSERT_NE(tab_strip_model->GetActiveWebContents(), originating_web_contents);

  base::RunLoop run_loop;
  auto show_dialog_callback =
      [&](std::unique_ptr<ExtensionInstallPromptShowParams> show_params,
          ExtensionInstallPrompt::DoneCallback done_callback,
          std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt) {
        // show_params_weak won't be dangling till the dialog is closed.
        ExtensionInstallPromptShowParams* show_params_weak = show_params.get();
        std::move(ExtensionInstallPrompt::GetDefaultShowDialogCallback())
            .Run(std::move(show_params), /*done_callback=*/base::DoNothing(),
                 std::move(prompt));

        // Ensure that show_params has the correct parent window and the
        // originating web contents are activated.
        EXPECT_TRUE(show_params_weak->GetParentWindow());
        EXPECT_EQ(show_params_weak->GetParentWindow(),
                  browser()->window()->GetNativeWindow());
        EXPECT_EQ(tab_strip_model->GetActiveWebContents(),
                  originating_web_contents);
        run_loop.Quit();
      };

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test extension").Build();
  ExtensionInstallPrompt prompt(originating_web_contents);
  prompt.ShowDialog(/*install_callback=*/base::DoNothing(), extension.get(),
                    /*icon=*/nullptr,
                    base::BindLambdaForTesting(show_dialog_callback));
  run_loop.Run();
}

// Verifies that the "Add extension" button is disabled initially, but
// re-enabled after a short time delay.
IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewTest, InstallButtonDelay) {
  ExtensionInstallDialogView::SetInstallButtonDelayForTesting(0);
  ExtensionInstallPromptTestHelper helper;
  ExtensionInstallDialogView* delegate_view = CreateAndShowPrompt(&helper);

  // Check that dialog is visible.
  EXPECT_TRUE(delegate_view->GetVisible());

  // Check initial button states.
  EXPECT_FALSE(
      delegate_view->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_TRUE(
      delegate_view->IsDialogButtonEnabled(ui::mojom::DialogButton::kCancel));
  EXPECT_TRUE(delegate_view->GetInitiallyFocusedView()->HasFocus());

  // Check OK button state after timeout to verify that it is re-enabled.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      delegate_view->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  // Ensure default button (cancel) has focus.
  EXPECT_TRUE(delegate_view->GetInitiallyFocusedView()->HasFocus());
  CloseAndWait(delegate_view->GetWidget());
}

// Regression test for https://crbug.com/1201031: Ensures that while an
// ExtensionInstallDialogView is visible, it does not (and cannot) refer to its
// originator tab/WebContents after the tab's closure.
//
// Note that the tab's closure is not typically possible by user interaction.
// However, it can occur by other means:
//   a) chrome.tabs.remove() or
//   b) window.close() from devtools (See repro in crbug).
// This test uses CloseWebContentsAt to mimic that behavior.
IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewTest,
                       TabClosureClearsWebContentsFromDialogView) {
  ExtensionInstallPromptTestHelper helper;
  ExtensionInstallDialogView* delegate_view = CreateAndShowPrompt(&helper);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* originator_contents =
      tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(delegate_view->GetShowParamsForTesting());
  EXPECT_EQ(originator_contents,
            delegate_view->GetShowParamsForTesting()->GetParentWebContents());

  // Add a second tab.
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);
  {
    // Close the first tab that results in install dialog moving to the second
    // tab.
    int tab1_idx = tab_strip_model->GetIndexOfWebContents(originator_contents);
    content::WebContentsDestroyedWatcher tab_destroyed_watcher(
        tab_strip_model->GetWebContentsAt(tab1_idx));
    int previous_tab_count = tab_strip_model->count();
    tab_strip_model->CloseWebContentsAt(tab1_idx, TabCloseTypes::CLOSE_NONE);
    EXPECT_EQ(previous_tab_count - 1, tab_strip_model->count());
    tab_destroyed_watcher.Wait();
  }

  class TabAddedObserver : public TabStripModelObserver {
   public:
    explicit TabAddedObserver(TabStripModel* tab_strip_model) {
      tab_strip_model->AddObserver(this);
    }

    void WaitForWebstoreTabAdded() { run_loop_.Run(); }

    // TabStripModelObserver:
    void OnTabStripModelChanged(
        TabStripModel* tab_strip_model,
        const TabStripModelChange& change,
        const TabStripSelectionChange& selection) override {
      if (change.type() != TabStripModelChange::kInserted)
        return;

      for (const auto& contents : change.GetInsert()->contents) {
        // Note: GetVisibleURL() is used instead of GetLastCommittedURL() for
        // simplicity's sake as this test doesn't serve webstore url and
        // the url doesn't commit.
        const GURL& url = contents.contents->GetVisibleURL();
        if (url.host() == extension_urls::GetNewWebstoreLaunchURL().host()) {
          run_loop_.Quit();
          return;
        }
      }
    }

   private:
    base::RunLoop run_loop_;
  };

  // The dialog remains visible even though |originator_contents| is gone. Note
  // that this doesn't seem quite intuitive, but this is how things are at the
  // moment. See crbug.com/1201031 for details.
  EXPECT_TRUE(delegate_view->GetVisible());

  EXPECT_EQ(nullptr,
            delegate_view->GetShowParamsForTesting()->GetParentWebContents());

  // Click webstore link.
  {
    TabAddedObserver observer(tab_strip_model);
    delegate_view->ClickLinkForTesting();
    observer.WaitForWebstoreTabAdded();
  }

  CloseAndWait(delegate_view->GetWidget());
}

class ExtensionInstallDialogViewInteractiveBrowserTest
    : public DialogBrowserTest {
 public:
  ExtensionInstallDialogViewInteractiveBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kAllowWithholdingExtensionPermissionsOnInstall);
  }

  ExtensionInstallDialogViewInteractiveBrowserTest(
      const ExtensionInstallDialogViewInteractiveBrowserTest&) = delete;
  ExtensionInstallDialogViewInteractiveBrowserTest& operator=(
      const ExtensionInstallDialogViewInteractiveBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    extensions::ChromeTestExtensionLoader loader(browser()->profile());
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    scoped_refptr<const extensions::Extension> extension = loader.LoadExtension(
        test_data_dir.AppendASCII("extensions/uitest/long_name"));

    SkBitmap icon;
    // The dialog will downscale large images.
    icon.allocN32Pixels(800, 800);
    icon.eraseARGB(255, 128, 255, 128);

    auto prompt = std::make_unique<ExtensionInstallPrompt::Prompt>(type_);
    // The invoke UI tests can either directly set permission messages to easily
    // test different potential edge cases, or use a proper permission set which
    // goes through the standard flow to generate the messages.
    if (permission_set_)
      prompt->AddPermissionSet(*permission_set_);
    else
      prompt->AddPermissionMessages(permission_messages_);

    if (from_webstore_)
      prompt->SetWebstoreData("69,420", true, 2.5, 37, "37");

    ExtensionInstallDialogView::SetInstallButtonDelayForTesting(0);
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    auto install_prompt =
        std::make_unique<ExtensionInstallPrompt>(web_contents);
    install_prompt->ShowDialog(base::DoNothing(), extension.get(), &icon,
                               std::move(prompt),
                               ExtensionInstallPrompt::ShowDialogCallback());
    base::RunLoop().RunUntilIdle();
  }

  void set_from_webstore() { from_webstore_ = true; }

  void set_type(ExtensionInstallPrompt::PromptType type) { type_ = type; }

  void SetPermissionSet(std::unique_ptr<PermissionSet> permissions) {
    permission_set_ = std::move(permissions);
  }

  void AddPermission(std::string permission) {
    permission_messages_.push_back(
        PermissionMessage(base::ASCIIToUTF16(permission), PermissionIDSet()));
  }

  void AddPermissionWithDetails(
      std::string main_permission,
      std::vector<std::u16string> detailed_permissions) {
    permission_messages_.push_back(
        PermissionMessage(base::ASCIIToUTF16(main_permission),
                          PermissionIDSet(), std::move(detailed_permissions)));
  }

 private:
  ExtensionInstallPrompt::PromptType type_ =
      ExtensionInstallPrompt::INSTALL_PROMPT;
  bool from_webstore_ = false;
  std::unique_ptr<PermissionSet> permission_set_;
  PermissionMessages permission_messages_;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_Simple) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_External) {
  set_type(ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_ExternalWithPermission) {
  set_type(ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT);
  AddPermission("Example permission");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_ReEnable) {
  set_type(ExtensionInstallPrompt::RE_ENABLE_PROMPT);
  AddPermission("Example permission");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_MultilinePermission) {
  AddPermission(
      "In the shade of the house, in the sunshine of the riverbank "
      "near the boats, in the shade of the Sal-wood forest, in the "
      "shade of the fig tree is where Siddhartha grew up");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_ManyPermissions) {
  for (int i = 0; i < 20; i++)
    AddPermission("Example permission");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_DetailedPermission) {
  AddPermissionWithDetails("Example header permission",
                           {u"Detailed permission 1", u"Detailed permission 2",
                            u"Very very very very very very long detailed "
                            u"permission that wraps to a new line"});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_WithWithholdingOption) {
  // The permissions withholding UI requires a proper permission set to be used,
  // as it checks for host permissions to determine if it should be shown.
  auto permissions = std::make_unique<PermissionSet>(
      extensions::APIPermissionSet(), extensions::ManifestPermissionSet(),
      extensions::URLPatternSet(
          {URLPattern(extensions::Extension::kValidHostPermissionSchemes,
                      "http://example.com/*")}),
      extensions::URLPatternSet());
  SetPermissionSet(std::move(permissions));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_AllInfoTypes) {
  AddPermission("Example permission");
  AddPermissionWithDetails(
      "This permission has details",
      {u"Detailed permission 1", u"Detailed permission 2"});
  ShowAndVerifyUi();
}

class ExtensionInstallDialogViewOnUninstallationTest
    : public ExtensionInstallDialogViewTest {
 public:
  ExtensionInstallDialogViewOnUninstallationTest() = default;
  ExtensionInstallDialogViewOnUninstallationTest(
      const ExtensionInstallDialogViewOnUninstallationTest&) = delete;
  ExtensionInstallDialogViewOnUninstallationTest& operator=(
      const ExtensionInstallDialogViewOnUninstallationTest&) = delete;

 protected:
  void UninstallExtension(const std::string& extension_id);
};

void ExtensionInstallDialogViewOnUninstallationTest::UninstallExtension(
    const std::string& extension_id) {
  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(browser()->profile()), extension_id);
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->UninstallExtension(
          extension_id,
          extensions::UninstallReason::UNINSTALL_REASON_FOR_TESTING, nullptr);
  observer.WaitForExtensionUninstalled();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewOnUninstallationTest,
                       UninstallingExtensionClosesDialog) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "install_prompt/dialog_on_uninstall/same_extension"));
  ASSERT_TRUE(extension);
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt =
      CreatePrompt(ExtensionInstallPrompt::REPAIR_PROMPT, extension);
  ExtensionInstallDialogView* dialog = new ExtensionInstallDialogView(
      std::make_unique<ExtensionInstallPromptShowParams>(web_contents()),
      base::DoNothing(), std::move(prompt));

  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      dialog, nullptr,
      platform_util::GetViewForWindow(browser()->window()->GetNativeWindow()));
  ASSERT_TRUE(modal_dialog);
  views::test::WidgetDestroyedWaiter dialog_observer(modal_dialog);
  modal_dialog->Show();
  EXPECT_FALSE(modal_dialog->IsClosed());
  UninstallExtension(extension->id());
  dialog_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewOnUninstallationTest,
                       UninstallingOtherExtensionDoesNotCloseDialog) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "install_prompt/dialog_on_uninstall/same_extension"));
  const extensions::Extension* other_extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "install_prompt/dialog_on_uninstall/other_extension"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(other_extension);
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt =
      CreatePrompt(ExtensionInstallPrompt::REPAIR_PROMPT, extension);
  ExtensionInstallDialogView* dialog = new ExtensionInstallDialogView(
      std::make_unique<ExtensionInstallPromptShowParams>(web_contents()),
      base::DoNothing(), std::move(prompt));

  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      dialog, nullptr,
      platform_util::GetViewForWindow(browser()->window()->GetNativeWindow()));
  ASSERT_TRUE(modal_dialog);
  views::test::WidgetDestroyedWaiter dialog_observer(modal_dialog);
  modal_dialog->Show();
  EXPECT_FALSE(modal_dialog->IsClosed());
  UninstallExtension(other_extension->id());
  ASSERT_TRUE(modal_dialog);
  modal_dialog->Close();
  dialog_observer.Wait();
}

class ExtensionInstallDialogRatingsSectionTest
    : public ExtensionInstallDialogViewTest {
 public:
  ExtensionInstallDialogRatingsSectionTest() {}

  ExtensionInstallDialogRatingsSectionTest(
      const ExtensionInstallDialogRatingsSectionTest&) = delete;
  ExtensionInstallDialogRatingsSectionTest& operator=(
      const ExtensionInstallDialogRatingsSectionTest&) = delete;

  void TestRatingsSectionA11y(int num_ratings,
                              double average_rating,
                              const std::string& expected_text);
};

void ExtensionInstallDialogRatingsSectionTest::TestRatingsSectionA11y(
    int num_ratings,
    double average_rating,
    const std::string& expected_text) {
  SCOPED_TRACE(base::StringPrintf(
      "Testing with %d ratings, %f average rating. Expected text: '%s'.",
      num_ratings, average_rating, expected_text.c_str()));
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt =
      CreatePrompt(ExtensionInstallPrompt::REPAIR_PROMPT);
  prompt->SetWebstoreData("1,234", true, average_rating, num_ratings,
                          base::NumberToString(num_ratings));

  ExtensionInstallDialogView* dialog = new ExtensionInstallDialogView(
      std::make_unique<ExtensionInstallPromptShowParams>(web_contents()),
      base::DoNothing(), std::move(prompt));

  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      dialog, nullptr,
      platform_util::GetViewForWindow(browser()->window()->GetNativeWindow()));
  modal_dialog->Show();

  views::View* rating_view = modal_dialog->non_client_view()->GetViewByID(
      ExtensionInstallDialogView::kRatingsViewId);
  ASSERT_TRUE(rating_view);
  {
    ui::AXNodeData node_data;
    rating_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
    EXPECT_EQ(ax::mojom::Role::kStaticText, node_data.role);
    EXPECT_EQ(expected_text,
              node_data.GetStringAttribute(ax::mojom::StringAttribute::kName));

    if (num_ratings == 0) {
      EXPECT_EQ(rating_view->GetViewAccessibility().GetCachedName(),
                l10n_util::GetStringUTF16(
                    IDS_EXTENSION_PROMPT_NO_RATINGS_ACCESSIBLE_TEXT));
    } else {
      EXPECT_EQ(rating_view->GetViewAccessibility().GetCachedName(),
                base::i18n::MessageFormatter::FormatWithNumberedArgs(
                    l10n_util::GetStringUTF16(
                        IDS_EXTENSION_PROMPT_RATING_ACCESSIBLE_TEXT),
                    average_rating, num_ratings));
    }
  }

  for (views::View* child : rating_view->children()) {
    ui::AXNodeData node_data;
    child->GetViewAccessibility().GetAccessibleNodeData(&node_data);
    EXPECT_EQ(ax::mojom::Role::kNone, node_data.role);
  }

  CloseAndWait(modal_dialog);
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogRatingsSectionTest,
                       RatingsSectionA11y) {
  TestRatingsSectionA11y(400, 3.297, "Rated 3.3 by 400 users.");
  TestRatingsSectionA11y(1, 1.0, "Rated 1.0 by one user.");
  TestRatingsSectionA11y(0, 0.0, "Not yet rated by any users.");
}

class ExtensionInstallDialogWithWithholdPermissionsUI
    : public ExtensionInstallDialogViewTestBase {
 public:
  ExtensionInstallDialogWithWithholdPermissionsUI() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kAllowWithholdingExtensionPermissionsOnInstall);
  }

  ExtensionInstallDialogWithWithholdPermissionsUI(
      const ExtensionInstallDialogWithWithholdPermissionsUI&) = delete;
  ExtensionInstallDialogWithWithholdPermissionsUI& operator=(
      const ExtensionInstallDialogWithWithholdPermissionsUI&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that some UI is displayed in the extra view when withholding
// permissions on installation.
IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogWithWithholdPermissionsUI,
                       ShowsWithholdUI) {
  ExtensionInstallPromptTestHelper helper;
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt =
      CreatePrompt(ExtensionInstallPrompt::INSTALL_PROMPT);
  // Add a permission to the prompt with example.com as an explicit pattern.
  PermissionSet permissions(
      extensions::APIPermissionSet(), extensions::ManifestPermissionSet(),
      extensions::URLPatternSet(
          {URLPattern(extensions::Extension::kValidHostPermissionSchemes,
                      "http://example.com/*")}),
      extensions::URLPatternSet());
  prompt->AddPermissionSet(permissions);
  auto dialog = std::make_unique<ExtensionInstallDialogView>(
      std::make_unique<ExtensionInstallPromptShowParams>(web_contents()),
      helper.GetCallback(), std::move(prompt));
  views::BubbleDialogDelegateView* delegate_view = dialog.get();

  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      dialog.release(), nullptr,
      platform_util::GetViewForWindow(browser()->window()->GetNativeWindow()));
  modal_dialog->Show();

  const views::View* const extra_view = delegate_view->GetExtraView();
  EXPECT_TRUE(extra_view);
  EXPECT_EQ("Checkbox", std::string(extra_view->GetClassName()));

  CloseAndWait(delegate_view->GetWidget());
}

class ExtensionInstallDialogViewRequestTest
    : public ExtensionInstallDialogViewTestBase {
 public:
  ExtensionInstallDialogViewRequestTest() = default;
  ExtensionInstallDialogViewRequestTest(
      const ExtensionInstallDialogViewRequestTest&) = delete;
  ExtensionInstallDialogViewRequestTest& operator=(
      const ExtensionInstallDialogViewRequestTest&) = delete;

 protected:
  ExtensionInstallDialogView* CreateAndShowRequestPrompt(
      ExtensionInstallPromptTestHelper* helper) {
    PermissionMessages permissions;
    permissions.push_back(
        PermissionMessage(u"Permission message", PermissionIDSet()));
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt =
        CreatePrompt(ExtensionInstallPrompt::EXTENSION_REQUEST_PROMPT);
    prompt->AddPermissionMessages(permissions);

    auto dialog = std::make_unique<ExtensionInstallDialogView>(
        std::make_unique<ExtensionInstallPromptShowParams>(web_contents()),
        helper->GetCallback(), std::move(prompt));
    ExtensionInstallDialogView* delegate_view = dialog.get();

    views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
        dialog.release(), nullptr,
        platform_util::GetViewForWindow(
            browser()->window()->GetNativeWindow()));
    modal_dialog->Show();

    return delegate_view;
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewRequestTest, NotifyDelegate) {
  base::HistogramTester histogram_tester;
  {
    // User presses "Send". Note that we have to wait for the 0ms delay for the
    // "Send" button to become enabled, hence the RunLoop later.
    ExtensionInstallDialogView::SetInstallButtonDelayForTesting(0);
    ExtensionInstallPromptTestHelper helper;
    ExtensionInstallDialogView* delegate_view =
        CreateAndShowRequestPrompt(&helper);
    EXPECT_TRUE(delegate_view->IsJustificationFieldVisibleForTesting());

    base::RunLoop().RunUntilIdle();
    delegate_view->AcceptDialog();
    EXPECT_EQ(ExtensionInstallPrompt::Result::ACCEPTED, helper.result());
    EXPECT_EQ(std::string(), helper.justification());
  }
  {
    // User presses cancel.
    ExtensionInstallPromptTestHelper helper;
    ExtensionInstallDialogView* delegate_view =
        CreateAndShowRequestPrompt(&helper);
    EXPECT_TRUE(delegate_view->IsJustificationFieldVisibleForTesting());
    delegate_view->CancelDialog();
    EXPECT_EQ(ExtensionInstallPrompt::Result::USER_CANCELED, helper.result());
    EXPECT_EQ(std::string(), helper.justification());
  }
  {
    // Dialog is closed without the user explicitly choosing to proceed or
    // cancel.
    ExtensionInstallPromptTestHelper helper;
    ExtensionInstallDialogView* delegate_view =
        CreateAndShowRequestPrompt(&helper);
    EXPECT_TRUE(delegate_view->IsJustificationFieldVisibleForTesting());
    // Note that the close button isn't present, but the dialog can still be
    // closed this way using Esc.
    EXPECT_FALSE(delegate_view->ShouldShowCloseButton());
    CloseAndWait(delegate_view->GetWidget());
    // TODO(devlin): Should this be ABORTED?
    EXPECT_EQ(ExtensionInstallPrompt::Result::USER_CANCELED, helper.result());
    EXPECT_EQ(std::string(), helper.justification());
  }
}

// Verifies that the "Send" button is disabled initially, but re-enabled after a
// short time delay.
IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewRequestTest,
                       RequestButtonDelay) {
  ExtensionInstallDialogView::SetInstallButtonDelayForTesting(0);
  ExtensionInstallPromptTestHelper helper;
  ExtensionInstallDialogView* delegate_view =
      CreateAndShowRequestPrompt(&helper);

  // Check that dialog and justification textfield are visible.
  EXPECT_TRUE(delegate_view->GetVisible());
  EXPECT_TRUE(delegate_view->IsJustificationFieldVisibleForTesting());

  // Check initial button states.
  EXPECT_FALSE(
      delegate_view->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  EXPECT_TRUE(
      delegate_view->IsDialogButtonEnabled(ui::mojom::DialogButton::kCancel));

  // Check OK button state after timeout to verify that it is re-enabled.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      delegate_view->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  CloseAndWait(delegate_view->GetWidget());
}

// Verifies that the "Send" button is disabled when the justification text
// exceeds the limit.
IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewRequestTest,
                       SendButtonDisabledWhenJustificationExceedsLimit) {
  ExtensionInstallDialogView::SetInstallButtonDelayForTesting(0);
  ExtensionInstallPromptTestHelper helper;
  ExtensionInstallDialogView* delegate_view =
      CreateAndShowRequestPrompt(&helper);

  // Check that dialog and justification textfield are visible.
  EXPECT_TRUE(delegate_view->GetVisible());
  ASSERT_TRUE(delegate_view->IsJustificationFieldVisibleForTesting());

  // Check OK button state after timeout to verify that it is re-enabled.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      delegate_view->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  // Add long justification and verify that OK button is disabled.
  delegate_view->SetJustificationTextForTesting(
      u"I really, really, really, really, really, really, really, really, "
      u"really, really, really, really, really, really, really, really, "
      u"really, really, really, really, really, really, really, really, "
      u"really, really, really, really, really, really, really, really, "
      u"really, really, really, really, really need this extension. Pretty "
      u"please!");
  EXPECT_FALSE(
      delegate_view->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  // Add short justificastion and verify that OK button is enabled.
  delegate_view->SetJustificationTextForTesting(u"I need it now.");
  EXPECT_TRUE(
      delegate_view->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  CloseAndWait(delegate_view->GetWidget());
}
