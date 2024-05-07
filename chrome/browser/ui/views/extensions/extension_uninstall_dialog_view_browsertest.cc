// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_urls.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/widget_test.h"

namespace {

const char kUninstallUrl[] = "https://www.google.com/";

const char kReferrerId[] = "chrome-remove-extension-dialog";

// A preference key storing the url loaded when an extension is uninstalled.
const char kUninstallUrlPrefKey[] = "uninstall_url";

scoped_refptr<const extensions::Extension> BuildTestExtension(
    const char* extension_name = "foo") {
  return extensions::ExtensionBuilder(extension_name).Build();
}

std::string GetActiveUrl(Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetLastCommittedURL()
      .spec();
}

void SetUninstallURL(extensions::ExtensionPrefs* prefs,
                     const std::string& extension_id) {
  prefs->UpdateExtensionPref(extension_id, kUninstallUrlPrefKey,
                             base::Value(kUninstallUrl));
}

void CloseUninstallDialog(views::Widget* const bubble_widget) {
  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble_widget);
  bubble_widget->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  destroyed_waiter.Wait();
}

class TestExtensionUninstallDialogDelegate
    : public extensions::ExtensionUninstallDialog::Delegate {
 public:
  explicit TestExtensionUninstallDialogDelegate(
      base::RepeatingClosure quit_closure)
      : quit_closure_(quit_closure) {}

  TestExtensionUninstallDialogDelegate(
      const TestExtensionUninstallDialogDelegate&) = delete;
  TestExtensionUninstallDialogDelegate& operator=(
      const TestExtensionUninstallDialogDelegate&) = delete;

  ~TestExtensionUninstallDialogDelegate() override {}

  bool canceled() const { return canceled_; }
  const std::u16string& error() const { return error_; }

 private:
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const std::u16string& error) override {
    ASSERT_FALSE(did_close_)
        << "OnExtensionUninstallDialogClosed() was called twice!";
    did_close_ = true;
    canceled_ = !did_start_uninstall;
    error_ = error;
    quit_closure_.Run();
  }

  base::RepeatingClosure quit_closure_;
  bool did_close_ = false;
  bool canceled_ = false;
  std::u16string error_;
};

}  // namespace

using ExtensionUninstallDialogViewBrowserTest = InProcessBrowserTest;

// Test that ExtensionUninstallDialog cancels the uninstall if the Window which
// is passed to ExtensionUninstallDialog::Create() is destroyed before
// ExtensionUninstallDialogDelegateView is created.
IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewBrowserTest,
                       TrackParentWindowDestruction) {
  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->AddExtension(extension.get());

  base::RunLoop run_loop;
  TestExtensionUninstallDialogDelegate delegate(run_loop.QuitClosure());
  std::unique_ptr<extensions::ExtensionUninstallDialog> dialog(
      extensions::ExtensionUninstallDialog::Create(
          browser()->profile(), browser()->window()->GetNativeWindow(),
          &delegate));
  browser()->window()->Close();
  content::RunAllPendingInMessageLoop();

  dialog->ConfirmUninstall(extension.get(),
                           extensions::UNINSTALL_REASON_FOR_TESTING,
                           extensions::UNINSTALL_SOURCE_FOR_TESTING);
  run_loop.Run();
  EXPECT_TRUE(delegate.canceled());
}

// Test that ExtensionUninstallDialog cancels the uninstall if the Window which
// is passed to ExtensionUninstallDialog::Create() is destroyed after
// ExtensionUninstallDialogDelegateView is created.
IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewBrowserTest,
                       TrackParentWindowDestructionAfterViewCreation) {
  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->AddExtension(extension.get());

  base::RunLoop run_loop;
  TestExtensionUninstallDialogDelegate delegate(run_loop.QuitClosure());
  std::unique_ptr<extensions::ExtensionUninstallDialog> dialog(
      extensions::ExtensionUninstallDialog::Create(
          browser()->profile(), browser()->window()->GetNativeWindow(),
          &delegate));
  content::RunAllPendingInMessageLoop();

  dialog->ConfirmUninstall(extension.get(),
                           extensions::UNINSTALL_REASON_FOR_TESTING,
                           extensions::UNINSTALL_SOURCE_FOR_TESTING);

  content::RunAllPendingInMessageLoop();

  // Kill parent window.
  browser()->window()->Close();
  run_loop.Run();
  EXPECT_TRUE(delegate.canceled());
}

// Tests uninstalling the extension while the dialog is active.
// Regression test for https://1200679.
IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewBrowserTest,
                       ExtensionUninstalledWhileDialogIsActive) {
  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());
  extensions::ExtensionService* const service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  service->AddExtension(extension.get());

  base::RunLoop run_loop;
  TestExtensionUninstallDialogDelegate delegate(run_loop.QuitClosure());
  std::unique_ptr<extensions::ExtensionUninstallDialog> dialog(
      extensions::ExtensionUninstallDialog::Create(
          browser()->profile(), browser()->window()->GetNativeWindow(),
          &delegate));

  dialog->ConfirmUninstall(extension.get(),
                           extensions::UNINSTALL_REASON_FOR_TESTING,
                           extensions::UNINSTALL_SOURCE_FOR_TESTING);
  // Wait for the icon to load and dialog to display.
  base::RunLoop().RunUntilIdle();

  service->UninstallExtension(
      extension->id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);

  run_loop.Run();
  // The dialog should be closed with an appropriate error.
  EXPECT_TRUE(delegate.canceled());
  EXPECT_EQ(u"Extension was removed before dialog closed.", delegate.error());

  // Explicitly destroy the dialog. ExtensionUninstallDialog's dtor will close
  // the view dialog if it's still open (which it shouldn't be), which will
  // guarantee that the check in the delegate for double-closed calls catches
  // any cases.
  dialog = nullptr;
}

// Test that we don't crash when uninstalling an extension from a web app
// window in Ash. Context: crbug.com/825554
IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewBrowserTest,
                       WebAppWindowAshCrash) {
  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->AddExtension(extension.get());

  std::unique_ptr<web_app::OsIntegrationTestOverrideBlockingRegistration>
      faked_os_integration;
  {
    base::ScopedAllowBlockingForTesting blocking;
    faked_os_integration = std::make_unique<
        web_app::OsIntegrationTestOverrideBlockingRegistration>();
  }
  const GURL start_url = GURL("https://test.com/");
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  web_app_info->scope = start_url;
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  webapps::AppId app_id = web_app::test::InstallWebApp(browser()->profile(),
                                                       std::move(web_app_info));
  Browser* app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);

  TestExtensionUninstallDialogDelegate delegate{base::DoNothing()};
  std::unique_ptr<extensions::ExtensionUninstallDialog> dialog;
  {
    base::RunLoop run_loop;
    dialog = extensions::ExtensionUninstallDialog::Create(
        app_browser->profile(), app_browser->window()->GetNativeWindow(),
        &delegate);
    run_loop.RunUntilIdle();
  }

  {
    base::RunLoop run_loop;
    dialog->ConfirmUninstall(extension.get(),
                             extensions::UNINSTALL_REASON_FOR_TESTING,
                             extensions::UNINSTALL_SOURCE_FOR_TESTING);
    run_loop.RunUntilIdle();
  }

  {
    base::ScopedAllowBlockingForTesting blocking;
    faked_os_integration.reset();
  }
}

class ParameterizedExtensionUninstallDialogViewBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<extensions::UninstallReason> {};

// Test that when the user clicks Uninstall on the ExtensionUninstallDialog the
// extension's uninstall url (when it is specified) should open and be the
// active tab.
IN_PROC_BROWSER_TEST_P(ParameterizedExtensionUninstallDialogViewBrowserTest,
                       EnsureExtensionUninstallURLIsActiveTabAfterUninstall) {
  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  extension_service->AddExtension(extension.get());
  SetUninstallURL(
      extensions::ExtensionPrefs::Get(extension_service->GetBrowserContext()),
      extension->id());

  // Auto-confirm the uninstall dialog.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  base::RunLoop run_loop;
  TestExtensionUninstallDialogDelegate delegate(run_loop.QuitClosure());
  std::unique_ptr<extensions::ExtensionUninstallDialog> dialog(
      extensions::ExtensionUninstallDialog::Create(
          browser()->profile(), browser()->window()->GetNativeWindow(),
          &delegate));
  content::RunAllPendingInMessageLoop();

  extensions::UninstallReason uninstall_reason = GetParam();
  dialog->ConfirmUninstall(extension, uninstall_reason,
                           extensions::UNINSTALL_SOURCE_FOR_TESTING);

  content::RunAllPendingInMessageLoop();

  // There should be 2 tabs open: chrome://about and the extension's uninstall
  // url.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  // This navigation can fail, since the uninstall url isn't hooked up to the
  // test server. That's fine, since we only care about the intended target,
  // which is valid.
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  // Verifying that the extension's uninstall url is the active tab.
  EXPECT_EQ(kUninstallUrl, GetActiveUrl(browser()));

  run_loop.Run();
  // The delegate should not be canceled because the user chose to uninstall
  // the extension, which should be successful.
  EXPECT_TRUE(!delegate.canceled());
}

// Test that when the user clicks the Report Abuse checkbox and clicks Uninstall
// on the ExtensionUninstallDialog, the extension's uninstall url (when it is
// specified) and the CWS Report Abuse survey are opened in the browser, also
// testing that the CWS survey is the active tab.
IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewBrowserTest,
                       EnsureCWSReportAbusePageIsActiveTabAfterUninstall) {
  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  SetUninstallURL(
      extensions::ExtensionPrefs::Get(extension_service->GetBrowserContext()),
      extension->id());
  extension_service->AddExtension(extension.get());

  // Auto-confirm the uninstall dialog.
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION);

  base::RunLoop run_loop;
  TestExtensionUninstallDialogDelegate delegate(run_loop.QuitClosure());
  std::unique_ptr<extensions::ExtensionUninstallDialog> dialog(
      extensions::ExtensionUninstallDialog::Create(
          browser()->profile(), browser()->window()->GetNativeWindow(),
          &delegate));
  content::RunAllPendingInMessageLoop();

  dialog->ConfirmUninstall(extension,
                           // UNINSTALL_REASON_USER_INITIATED is used to trigger
                           // complete uninstallation.
                           extensions::UNINSTALL_REASON_USER_INITIATED,
                           extensions::UNINSTALL_SOURCE_FOR_TESTING);

  content::RunAllPendingInMessageLoop();
  // There should be 3 tabs open: chrome://about, the extension's uninstall url,
  // and the CWS Report Abuse survey.
  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  // This navigation can fail, since the webstore report abuse url isn't hooked
  // up to the test server. That's fine, since we only care about the intended
  // target, which is valid.
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  // The CWS Report Abuse survey should be the active tab.
  EXPECT_EQ(
      extension_urls::GetWebstoreReportAbuseUrl(extension->id(), kReferrerId),
      GetActiveUrl(browser()));
  // Similar to the scenario above, this navigation can fail. The uninstall url
  // isn't hooked up to our test server.
  content::WaitForLoadStop(browser()->tab_strip_model()->GetWebContentsAt(1));
  // Verifying that the extension's uninstall url was opened. It should not be
  // the active tab.
  EXPECT_EQ(kUninstallUrl, browser()
                               ->tab_strip_model()
                               ->GetWebContentsAt(1)
                               ->GetLastCommittedURL()
                               .spec());

  run_loop.Run();
  // The delegate should not be canceled because the user chose to uninstall the
  // extension, which should be successful.
  EXPECT_TRUE(!delegate.canceled());
}

// Tests the dialog is anchored in the correct place based on whether the
// extensions container is visible.
// Regression test for crbug.com/133249.
IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewBrowserTest,
                       DialogAnchoredInCorrectPlace) {
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();

  scoped_refptr<const extensions::Extension> extensionA(
      BuildTestExtension("Extension A"));
  scoped_refptr<const extensions::Extension> extensionB(
      BuildTestExtension("Extension B"));
  extension_service->AddExtension(extensionA.get());
  extension_service->AddExtension(extensionB.get());

  // Extensions container should be visible since there are enabled
  // extensions.
  ExtensionsToolbarContainer* const container =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar()
          ->extensions_container();
  ASSERT_TRUE(container->GetVisible());
  ASSERT_TRUE(container->GetViewForId(extensionA->id()));

  {
    base::RunLoop run_loop;
    TestExtensionUninstallDialogDelegate delegate(run_loop.QuitClosure());
    std::unique_ptr<extensions::ExtensionUninstallDialog> dialog(
        extensions::ExtensionUninstallDialog::Create(
            browser()->profile(), browser()->window()->GetNativeWindow(),
            &delegate));

    dialog->ConfirmUninstall(extensionA.get(),
                             extensions::UNINSTALL_REASON_FOR_TESTING,
                             extensions::UNINSTALL_SOURCE_FOR_TESTING);
    // Wait for the icon to load and dialog to display.
    base::RunLoop().RunUntilIdle();

    // Dialog should be anchored to the container if the container is visible
    // and the extension has an action view.
    views::Widget* const bubble_widget =
        container->GetAnchoredWidgetForExtensionForTesting(extensionA->id());
    EXPECT_TRUE(bubble_widget);

    CloseUninstallDialog(bubble_widget);
    EXPECT_FALSE(
        container->GetAnchoredWidgetForExtensionForTesting(extensionA->id()));
  }

  // Disable the extension so it doesn't have an action view, but the container
  // is still visible.
  extension_service->DisableExtension(
      extensionA->id(), extensions::disable_reason::DISABLE_USER_ACTION);
  ASSERT_TRUE(container->GetVisible());
  ASSERT_FALSE(container->GetViewForId(extensionA->id()));

  {
    base::RunLoop run_loop;
    TestExtensionUninstallDialogDelegate delegate(run_loop.QuitClosure());
    std::unique_ptr<extensions::ExtensionUninstallDialog> dialog(
        extensions::ExtensionUninstallDialog::Create(
            browser()->profile(), browser()->window()->GetNativeWindow(),
            &delegate));

    dialog->ConfirmUninstall(extensionA.get(),
                             extensions::UNINSTALL_REASON_FOR_TESTING,
                             extensions::UNINSTALL_SOURCE_FOR_TESTING);
    base::RunLoop().RunUntilIdle();

    // Dialog should be anchored to the container if the container is visible
    // and the extension does not have an action view.
    views::Widget* const bubble_widget =
        container->GetAnchoredWidgetForExtensionForTesting(extensionA->id());
    EXPECT_TRUE(bubble_widget);

    CloseUninstallDialog(bubble_widget);
    EXPECT_FALSE(
        container->GetAnchoredWidgetForExtensionForTesting(extensionA->id()));
  }

  // Disable the second extension to have all extensions disable and the
  // container hidden.
  extension_service->DisableExtension(
      extensionB->id(), extensions::disable_reason::DISABLE_USER_ACTION);
  views::test::WaitForAnimatingLayoutManager(container);
  ASSERT_FALSE(container->GetVisible());
  ASSERT_FALSE(container->GetViewForId(extensionB->id()));

  {
    base::RunLoop run_loop;
    TestExtensionUninstallDialogDelegate delegate(run_loop.QuitClosure());
    std::unique_ptr<extensions::ExtensionUninstallDialog> dialog(
        extensions::ExtensionUninstallDialog::Create(
            browser()->profile(), browser()->window()->GetNativeWindow(),
            &delegate));

    dialog->ConfirmUninstall(extensionA.get(),
                             extensions::UNINSTALL_REASON_FOR_TESTING,
                             extensions::UNINSTALL_SOURCE_FOR_TESTING);
    base::RunLoop().RunUntilIdle();

    // Dialog should be modal if the container is not visible, which means it is
    // not anchored to the container.
    views::Widget* const bubble_widget =
        container->GetAnchoredWidgetForExtensionForTesting(extensionA->id());
    EXPECT_FALSE(bubble_widget);
  }
}

class ExtensionUninstallDialogViewInteractiveBrowserTest
    : public DialogBrowserTest {
 public:
  enum UninstallMethod {
    MANUAL_UNINSTALL,
    UNINSTALL_BY_EXTENSION,
  };
  enum ExtensionOrigin {
    EXTENSION_LOCAL_SOURCE,
    EXTENSION_FROM_WEBSTORE,
  };
  void ShowUi(const std::string& name) override {
    extensions::ExtensionBuilder extension_builder("ExtensionForRemoval");
    if (extension_origin_ == EXTENSION_FROM_WEBSTORE) {
      extension_builder.SetManifestKey(
          "update_url", extension_urls::GetWebstoreUpdateUrl().spec());
    }

    extension_ = extension_builder.Build();
    extensions::ExtensionSystem::Get(browser()->profile())
        ->extension_service()
        ->AddExtension(extension_.get());

    dialog_ = extensions::ExtensionUninstallDialog::Create(
        browser()->profile(), browser()->window()->GetNativeWindow(),
        &delegate_);
    if (uninstall_method_ == UNINSTALL_BY_EXTENSION) {
      triggering_extension_ =
          extensions::ExtensionBuilder("TestExtensionRemover").Build();
      dialog_->ConfirmUninstallByExtension(
          extension_.get(), triggering_extension_.get(),
          extensions::UNINSTALL_REASON_FOR_TESTING,
          extensions::UNINSTALL_SOURCE_FOR_TESTING);
    } else {
      dialog_->ConfirmUninstall(extension_.get(),
                                extensions::UNINSTALL_REASON_FOR_TESTING,
                                extensions::UNINSTALL_SOURCE_FOR_TESTING);
    }

    // The dialog shows when an icon update happens, run all pending messages to
    // make sure that the widget exists and is showing at the end of this call.
    content::RunAllPendingInMessageLoop();
  }

  void RunTest(UninstallMethod uninstall_method,
               ExtensionOrigin extension_origin) {
    uninstall_method_ = uninstall_method;
    extension_origin_ = extension_origin;

    ShowAndVerifyUi();
  }

 private:
  class TestDelegate : public extensions::ExtensionUninstallDialog::Delegate {
    void OnExtensionUninstallDialogClosed(
        bool did_start_uninstall,
        const std::u16string& error) override {}
  };

  void TearDownOnMainThread() override {
    // Dialog holds references to the profile, so it needs to tear down before
    // profiles are deleted.
    dialog_.reset();
  }

  scoped_refptr<const extensions::Extension> extension_;
  scoped_refptr<const extensions::Extension> triggering_extension_;
  TestDelegate delegate_;
  std::unique_ptr<extensions::ExtensionUninstallDialog> dialog_;

  UninstallMethod uninstall_method_;
  ExtensionOrigin extension_origin_;
};

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/40069124): Enable the test again.
#define MAYBE_InvokeUi_ManualUninstall DISABLED_InvokeUi_ManualUninstall
#else
#define MAYBE_InvokeUi_ManualUninstall InvokeUi_ManualUninstall
#endif
IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewInteractiveBrowserTest,
                       MAYBE_InvokeUi_ManualUninstall) {
  RunTest(MANUAL_UNINSTALL, EXTENSION_LOCAL_SOURCE);
}

// TODO(crbug.com/40926539): Re-enable this test
#if BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_ManualUninstallShowReportAbuse \
  DISABLED_InvokeUi_ManualUninstallShowReportAbuse
#else
#define MAYBE_InvokeUi_ManualUninstallShowReportAbuse \
  InvokeUi_ManualUninstallShowReportAbuse
#endif
IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewInteractiveBrowserTest,
                       MAYBE_InvokeUi_ManualUninstallShowReportAbuse) {
  RunTest(MANUAL_UNINSTALL, EXTENSION_FROM_WEBSTORE);
}

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/40069124): Enable the test again.
#define MAYBE_InvokeUi_UninstallByExtension \
  DISABLED_InvokeUi_UninstallByExtension
#else
#define MAYBE_InvokeUi_UninstallByExtension InvokeUi_UninstallByExtension
#endif
IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewInteractiveBrowserTest,
                       MAYBE_InvokeUi_UninstallByExtension) {
  RunTest(UNINSTALL_BY_EXTENSION, EXTENSION_LOCAL_SOURCE);
}

// TODO(crbug.com/40926539): Fix flakiness and re-enable this test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_UninstallByExtensionShowReportAbuse \
  DISABLED_InvokeUi_UninstallByExtensionShowReportAbuse
#else
#define MAYBE_InvokeUi_UninstallByExtensionShowReportAbuse \
  InvokeUi_UninstallByExtensionShowReportAbuse
#endif
IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewInteractiveBrowserTest,
                       MAYBE_InvokeUi_UninstallByExtensionShowReportAbuse) {
  RunTest(UNINSTALL_BY_EXTENSION, EXTENSION_FROM_WEBSTORE);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ParameterizedExtensionUninstallDialogViewBrowserTest,
    testing::Values(extensions::UNINSTALL_REASON_USER_INITIATED,
                    extensions::UNINSTALL_REASON_CHROME_WEBSTORE));
