// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_TEST_BASE_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_TEST_BASE_H_

#include <memory>

#include "base/files/file_path.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#include "base/test/scoped_path_override.h"
#endif  // BUILDFLAG(IS_WIN)

namespace extensions {
class Extension;
class ScopedTestDialogAutoConfirm;
}  // namespace extensions

// C++ base class used by Extensions tests in chrome/test/data/webui/extensions/
// and chrome/browser/ui/webui/extensions/.
class ExtensionSettingsTestBase : public WebUIMochaBrowserTest {
 public:
  ExtensionSettingsTestBase();

  ExtensionSettingsTestBase(const ExtensionSettingsTestBase&) = delete;
  ExtensionSettingsTestBase& operator=(const ExtensionSettingsTestBase&) =
      delete;

  ~ExtensionSettingsTestBase() override;

 protected:
  void InstallGoodExtension();

  void InstallErrorsExtension();

  void InstallSharedModule();

  void InstallPackagedApp();

  void InstallHostedApp();

  void InstallPlatformApp();

  // Installs chrome/test/data/extensions/options_page_in_view extension
  // and returns it back to the caller.  Can return null upon failure.
  const extensions::Extension* InstallExtensionWithInPageOptions();

  void SetAutoConfirmUninstall();

  // Sets the DevMode status for the current profile.
  void SetDevModeEnabled(bool enabled);

  // Sets whether to ignore errors for deprecated manifest versions.
  void SetSilenceDeprecatedManifestVersionWarnings(bool silence);

  const base::FilePath& test_data_dir() { return test_data_dir_; }

 private:
  const extensions::Extension* InstallExtension(const base::FilePath& path);

  const base::FilePath test_data_dir_;

#if BUILDFLAG(IS_WIN)
  // This is needed to stop tests creating a shortcut in the Windows start menu.
  // The override needs to last until the test is destroyed, because Windows
  // shortcut tasks which create the shortcut can run after the test body
  // returns.
  base::ScopedPathOverride override_start_dir{base::DIR_START_MENU};
#endif  // BUILDFLAG(IS_WIN)

  // Disable extension content verification.
  extensions::ScopedIgnoreContentVerifierForTest ignore_content_verification_;

  // Disable extension install verification.
  extensions::ScopedInstallVerifierBypassForTest ignore_install_verification_;

  std::unique_ptr<extensions::ScopedTestDialogAutoConfirm>
      uninstall_auto_confirm_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_TEST_BASE_H_
