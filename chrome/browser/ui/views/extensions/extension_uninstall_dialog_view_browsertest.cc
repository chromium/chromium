// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/value_builder.h"

namespace {

const char kUninstallUrl[] = "https://www.google.com/";

const char kReferrerId[] = "chrome-remove-extension-dialog";

// A preference key storing the url loaded when an extension is uninstalled.
const char kUninstallUrlPrefKey[] = "uninstall_url";

scoped_refptr<const extensions::Extension> BuildTestExtension() {
  return extensions::ExtensionBuilder("foo").Build();
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
                             std::make_unique<base::Value>(kUninstallUrl));
}

class TestExtensionUninstallDialogDelegate
    : public extensions::ExtensionUninstallDialog::Delegate {
 public:
  explicit TestExtensionUninstallDialogDelegate(
      const base::Closure& quit_closure)
      : quit_closure_(quit_closure), canceled_(false) {}

  ~TestExtensionUninstallDialogDelegate() override {}

  bool canceled() { return canceled_; }

 private:
  void OnExtensionUninstallDialogClosed(bool did_start_uninstall,
                                        const base::string16& error) override {
    canceled_ = !did_start_uninstall;
    quit_closure_.Run();
  }

  base::Closure quit_closure_;
  bool canceled_;

  DISALLOW_COPY_AND_ASSIGN(TestExtensionUninstallDialogDelegate);
};

}  // namespace

typedef InProcessBrowserTest ExtensionUninstallDialogViewBrowserTest;

// Test that ExtensionUninstallDialog cancels the uninstall if the Window which
// is passed to ExtensionUninstallDialog::Create() is destroyed before
// ExtensionUninstallDialogDelegateView is created.
IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewBrowserTest,
                       TrackParentWindowDestruction) {
  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());
  extensions::ExtensionSystem::Get(browser()->profile())->extension_service()
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
  extensions::ExtensionSystem::Get(browser()->profile())->extension_service()
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

#if defined(OS_CHROMEOS)
// Test that we don't crash when uninstalling an extension from a bookmark app
// window in Ash. Context: crbug.com/825554
IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewBrowserTest,
                       BookmarkAppWindowAshCrash) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(features::kDesktopPWAWindowing);

  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->AddExtension(extension.get());

  WebApplicationInfo info;
  info.app_url = GURL("https://test.com/");
  const extensions::Extension* bookmark_app =
      extensions::browsertest_util::InstallBookmarkApp(browser()->profile(),
                                                       info);
  Browser* app_browser = extensions::browsertest_util::LaunchAppBrowser(
      browser()->profile(), bookmark_app);

  std::unique_ptr<extensions::ExtensionUninstallDialog> dialog;
  {
    base::RunLoop run_loop;
    dialog.reset(extensions::ExtensionUninstallDialog::Create(
        app_browser->profile(), app_browser->window()->GetNativeWindow(),
        nullptr));
    run_loop.RunUntilIdle();
  }

  {
    base::RunLoop run_loop;
    dialog->ConfirmUninstall(extension.get(),
                             extensions::UNINSTALL_REASON_FOR_TESTING,
                             extensions::UNINSTALL_SOURCE_FOR_TESTING);
    run_loop.RunUntilIdle();
  }
}
#endif  // defined(OS_CHROMEOS)

// Test that when the user clicks Uninstall on the ExtensionUninstallDialog, the
// extension's uninstall url (when it is specified) should open and be the
// active tab.
// TODO(catmullings): Disabled due to flake on Win 10 x64.
// https://crbug.com/725197
IN_PROC_BROWSER_TEST_F(
    ExtensionUninstallDialogViewBrowserTest,
    DISABLED_EnsureExtensionUninstallURLIsActiveTabAfterUninstall) {
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

  dialog->ConfirmUninstall(extension,
                           // UNINSTALL_REASON_USER_INITIATED is used to trigger
                           // complete uninstallation.
                           extensions::UNINSTALL_REASON_USER_INITIATED,
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
  // The delegate should not be canceled because the user chose to uninstall the
  // extension, which should be successful.
  EXPECT_TRUE(!delegate.canceled());
}

// Test that when the user clicks the Report Abuse checkbox and clicks Uninstall
// on the ExtensionUninstallDialog, the extension's uninstall url (when it is
// specified) and the CWS Report Abuse survey are opened in the browser, also
// testing that the CWS survey is the active tab.
// TODO(catmullings): Disabled due to flake on Windows and Linux.
// http://crbug.com/725197
IN_PROC_BROWSER_TEST_F(
    ExtensionUninstallDialogViewBrowserTest,
    DISABLED_EnsureCWSReportAbusePageIsActiveTabAfterUninstall) {
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

    dialog_.reset(extensions::ExtensionUninstallDialog::Create(
        browser()->profile(), browser()->window()->GetNativeWindow(),
        &delegate_));
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
        const base::string16& error) override {}
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

IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewInteractiveBrowserTest,
                       InvokeUi_ManualUninstall) {
  RunTest(MANUAL_UNINSTALL, EXTENSION_LOCAL_SOURCE);
}

IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewInteractiveBrowserTest,
                       InvokeUi_ManualUninstallShowReportAbuse) {
  RunTest(MANUAL_UNINSTALL, EXTENSION_FROM_WEBSTORE);
}

IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewInteractiveBrowserTest,
                       InvokeUi_UninstallByExtension) {
  RunTest(UNINSTALL_BY_EXTENSION, EXTENSION_LOCAL_SOURCE);
}

IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewInteractiveBrowserTest,
                       InvokeUi_UninstallByExtensionShowReportAbuse) {
  RunTest(UNINSTALL_BY_EXTENSION, EXTENSION_FROM_WEBSTORE);
}
