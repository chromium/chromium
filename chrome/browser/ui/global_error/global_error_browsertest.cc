// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_disabled_ui.h"
#include "chrome/browser/extensions/extension_error_controller.h"
#include "chrome/browser/extensions/extension_error_ui_default.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_install_error.h"
#include "chrome/browser/extensions/test_blocklist.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/recovery/recovery_install_global_error.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error/global_error_observer.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/crx_verifier.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/mock_external_provider.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"

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

// Helper to wait for a global error to be added. To stop waiting, the global
// error must have a bubble view.
class GlobalErrorWaiter : public GlobalErrorObserver {
 public:
  explicit GlobalErrorWaiter(Profile* profile)
      : service_(GlobalErrorServiceFactory::GetForProfile(profile)) {
    scoped_observation_.Observe(service_.get());
  }

  GlobalErrorWaiter(const GlobalErrorWaiter&) = delete;
  GlobalErrorWaiter& operator=(const GlobalErrorWaiter&) = delete;

  ~GlobalErrorWaiter() override = default;

  // GlobalErrorObserver
  void OnGlobalErrorsChanged() override {
    if (service_->GetFirstGlobalErrorWithBubbleView())
      run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
  raw_ptr<GlobalErrorService> service_;
  base::ScopedObservation<GlobalErrorService, GlobalErrorObserver>
      scoped_observation_{this};
};

}  // namespace

class GlobalErrorBubbleTest : public DialogBrowserTest {
 public:
  GlobalErrorBubbleTest() {
    extensions::ExtensionPrefs::SetRunAlertsInFirstRunForTest();
  }

  GlobalErrorBubbleTest(const GlobalErrorBubbleTest&) = delete;
  GlobalErrorBubbleTest& operator=(const GlobalErrorBubbleTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override;
};

void GlobalErrorBubbleTest::ShowUi(const std::string& name) {
  Profile* profile = browser()->profile();
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);

  extensions::ExtensionBuilder builder("Browser Action");
  builder.SetAction(extensions::ActionInfo::Type::kBrowser);
  builder.SetLocation(extensions::mojom::ManifestLocation::kInternal);
  scoped_refptr<const extensions::Extension> test_extension = builder.Build();
  extension_service->AddExtension(test_extension.get());

  if (name == "ExtensionDisabledGlobalError") {
    GlobalErrorWaiter waiter(profile);
    extensions::AddExtensionDisabledError(extension_service,
                                          test_extension.get(), false);
    waiter.Wait();
    ShowPendingError(browser());
  } else if (name == "ExtensionDisabledGlobalErrorRemote") {
    GlobalErrorWaiter waiter(profile);
    extensions::AddExtensionDisabledError(extension_service,
                                          test_extension.get(), true);
    waiter.Wait();
    ShowPendingError(browser());
  } else if (name == "ExtensionGlobalError") {
    extensions::TestBlocklist test_blocklist(
        extensions::Blocklist::Get(profile));
    extension_registry->AddBlocklisted(test_extension);
    // Only BLOCKLISTED_MALWARE results in a bubble displaying to the user.
    // Other types are greylisted, not blocklisted.
    test_blocklist.SetBlocklistState(test_extension->id(),
                                     extensions::BLOCKLISTED_MALWARE, true);
    // Ensure ExtensionService::ManageBlocklist() runs, which shows the dialog.
    // (This flow doesn't use OnGlobalErrorsChanged.) This is asynchronous, and
    // using TestBlocklist ensures the tasks run without delay, but some tasks
    // run on the IO thread, so post a task there to ensure it was flushed. The
    // test also needs to invoke OnBlocklistUpdated() directly. Usually this
    // happens via a callback from the SafeBrowsing DB, but TestBlocklist
    // replaced the SafeBrowsing DB with a fake one, so the notification source
    // is different.
    static_cast<extensions::Blocklist::Observer*>(extension_service)
        ->OnBlocklistUpdated();
    base::RunLoop().RunUntilIdle();
    base::RunLoop flush_io;
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), flush_io.QuitClosure());
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

    GlobalErrorWaiter waiter(profile);
    auto provider = std::make_unique<extensions::MockExternalProvider>(
        extension_service, extensions::mojom::ManifestLocation::kExternalPref);
    extensions::MockExternalProvider* provider_ptr = provider.get();
    extension_service->AddProviderForTesting(std::move(provider));
    provider_ptr->UpdateOrAddExtension(kExtensionWithUpdateUrl, "1.0.0.0",
                                       crx_path);
    extension_service->CheckForExternalUpdates();

    // ExternalInstallError::OnDialogReady() adds the error and shows the dialog
    // immediately.
    waiter.Wait();
  } else if (name == "RecoveryInstallGlobalError") {
    GlobalErrorWaiter waiter(profile);
    g_browser_process->local_state()->SetBoolean(
        prefs::kRecoveryComponentNeedsElevation, true);
    waiter.Wait();
    ShowPendingError(browser());
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
  extensions::SandboxedUnpacker::ScopedVerifierFormatOverrideForTest
      verifier_format_override(crx_file::VerifierFormat::CRX3);
  extensions::FeatureSwitch::ScopedOverride prompt(
      extensions::FeatureSwitch::prompt_for_external_extensions(), true);
  ShowAndVerifyUi();
}

// RecoveryInstallGlobalError only exists on Windows and Mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(GlobalErrorBubbleTest,
                       InvokeUi_RecoveryInstallGlobalError) {
  ShowAndVerifyUi();
}
#endif
