// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_disabled_ui.h"
#include "chrome/browser/extensions/extension_error_controller.h"
#include "chrome/browser/extensions/extension_error_ui_default.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_install_error.h"
#include "chrome/browser/extensions/test_blacklist.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/recovery/recovery_install_global_error.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/mock_external_provider.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/signin/signin_global_error_factory.h"
#endif

namespace {

// Shows the first GlobalError with associated UI associated with |browser|.
void ShowPendingError(Browser* browser) {
  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(browser->profile());
  GlobalError* error = service->GetFirstGlobalErrorWithBubbleView();
  ASSERT_TRUE(error);
  error->ShowBubbleView(browser);
}

// Packs an extension from the extensions test data folder into a crx.
base::FilePath PackCRXInTempDir(base::ScopedTempDir* temp_dir,
                                const char* extension_folder,
                                const char* pem_file) {
  EXPECT_TRUE(temp_dir->CreateUniqueTempDir());
  base::FilePath crx_path = temp_dir->GetPath().AppendASCII("temp.crx");

  base::FilePath test_data;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
  test_data = test_data.AppendASCII("extensions");

  base::FilePath dir_path = test_data.AppendASCII(extension_folder);
  base::FilePath pem_path = test_data.AppendASCII(pem_file);

  EXPECT_TRUE(extensions::ExtensionCreator().Run(
      dir_path, crx_path, pem_path, base::FilePath(),
      extensions::ExtensionCreator::kOverwriteCRX));
  EXPECT_TRUE(base::PathExists(crx_path));
  return crx_path;
}

}  // namespace

class GlobalErrorBubbleTest : public DialogBrowserTest {
 public:
  GlobalErrorBubbleTest() {
    extensions::ExtensionPrefs::SetRunAlertsInFirstRunForTest();
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GlobalErrorBubbleTest);
};

void GlobalErrorBubbleTest::ShowUi(const std::string& name) {
  content::WindowedNotificationObserver global_errors_updated(
      chrome::NOTIFICATION_GLOBAL_ERRORS_CHANGED,
      base::Bind([](const content::NotificationSource& source,
                    const content::NotificationDetails& details) -> bool {
        return content::Details<GlobalError>(details).ptr()->HasBubbleView();
      }));
  Profile* profile = browser()->profile();
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);

  extensions::ExtensionBuilder builder("Browser Action");
  builder.SetAction(extensions::ExtensionBuilder::ActionType::BROWSER_ACTION);
  builder.SetLocation(extensions::Manifest::INTERNAL);
  scoped_refptr<const extensions::Extension> test_extension = builder.Build();
  extension_service->AddExtension(test_extension.get());

  if (name == "ExtensionDisabledGlobalError") {
    extensions::AddExtensionDisabledError(extension_service,
                                          test_extension.get(), false);
    global_errors_updated.Wait();
    ShowPendingError(browser());
  } else if (name == "ExtensionDisabledGlobalErrorRemote") {
    extensions::AddExtensionDisabledError(extension_service,
                                          test_extension.get(), true);
    global_errors_updated.Wait();
    ShowPendingError(browser());
  } else if (name == "ExtensionGlobalError") {
    extensions::TestBlacklist test_blacklist(
        extensions::Blacklist::Get(profile));
    extension_registry->AddBlacklisted(test_extension);
    // Only BLACKLISTED_MALWARE results in a bubble displaying to the user.
    // Other types are greylisted, not blacklisted.
    test_blacklist.SetBlacklistState(test_extension->id(),
                                     extensions::BLACKLISTED_MALWARE, true);
    // Ensure ExtensionService::ManageBlacklist() runs, which shows the dialog.
    // (This flow doesn't use NOTIFICATION_GLOBAL_ERRORS_CHANGED.) This is
    // asynchronous, and using TestBlacklist ensures the tasks run without
    // delay, but some tasks run on the IO thread, so post a task there to
    // ensure it was flushed.
    // The test also needs to invoke OnBlacklistUpdated() directly. Usually this
    // happens via a callback from the SafeBrowsing DB, but TestBlacklist
    // replaced the SafeBrowsing DB with a fake one, so the notification source
    // is different.
    static_cast<extensions::Blacklist::Observer*>(extension_service)
        ->OnBlacklistUpdated();
    base::RunLoop().RunUntilIdle();
    base::RunLoop flush_io;
    base::PostTaskWithTraitsAndReply(FROM_HERE, {content::BrowserThread::IO},
                                     base::DoNothing(), flush_io.QuitClosure());
    flush_io.Run();

    // Oh no! This relies on RunUntilIdle() to show the bubble. The bubble is
    // not persistent, so events from the OS can also cause the bubble to close
    // in the following call.
    base::RunLoop().RunUntilIdle();
  } else if (name == "ExternalInstallBubbleAlert") {
    // To trigger a bubble alert (rather than a menu alert), the extension must
    // come from the webstore, which needs the update to come from a signed crx.
    const char kExtensionWithUpdateUrl[] = "akjooamlhcgeopfifcmlggaebeocgokj";
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ScopedTempDir temp_dir;
    base::FilePath crx_path = PackCRXInTempDir(
        &temp_dir, "update_from_webstore", "update_from_webstore.pem");

    auto provider = std::make_unique<extensions::MockExternalProvider>(
        extension_service, extensions::Manifest::EXTERNAL_PREF);
    extensions::MockExternalProvider* provider_ptr = provider.get();
    extension_service->AddProviderForTesting(std::move(provider));
    provider_ptr->UpdateOrAddExtension(kExtensionWithUpdateUrl, "1.0.0.0",
                                       crx_path);
    extension_service->CheckForExternalUpdates();

    // ExternalInstallError::OnDialogReady() adds the error and shows the dialog
    // immediately.
    global_errors_updated.Wait();
  } else if (name == "RecoveryInstallGlobalError") {
    g_browser_process->local_state()->SetBoolean(
        prefs::kRecoveryComponentNeedsElevation, true);
    global_errors_updated.Wait();
    ShowPendingError(browser());

#if !defined(OS_CHROMEOS)
  } else if (name == "SigninGlobalError") {
    SigninGlobalErrorFactory::GetForProfile(profile)->ShowBubbleView(browser());
#endif
  } else {
    ADD_FAILURE();
  }
}

IN_PROC_BROWSER_TEST_F(GlobalErrorBubbleTest,
                       InvokeUi_ExtensionDisabledGlobalError) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GlobalErrorBubbleTest,
                       InvokeUi_ExtensionDisabledGlobalErrorRemote) {
  ShowAndVerifyUi();
}

// This shows a non-persistent dialog during a RunLoop::RunUntilIdle(), so it's
// not possible to guarantee that events to dismiss the dialog are not processed
// as well. Disable by default to prevent flakiness in browser_tests.
IN_PROC_BROWSER_TEST_F(GlobalErrorBubbleTest,
                       DISABLED_InvokeUi_ExtensionGlobalError) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GlobalErrorBubbleTest,
                       InvokeUi_ExternalInstallBubbleAlert) {
  extensions::FeatureSwitch::ScopedOverride prompt(
      extensions::FeatureSwitch::prompt_for_external_extensions(), true);
  ShowAndVerifyUi();
}

// RecoveryInstallGlobalError only exists on Windows and Mac.
#if defined(OS_WIN) || defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(GlobalErrorBubbleTest,
                       InvokeUi_RecoveryInstallGlobalError) {
  ShowAndVerifyUi();
}
#endif

// Signin global errors never happon on ChromeOS.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(GlobalErrorBubbleTest, InvokeUi_SigninGlobalError) {
  ShowAndVerifyUi();
}
#endif
