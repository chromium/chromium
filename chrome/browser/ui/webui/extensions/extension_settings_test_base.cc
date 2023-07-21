// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_settings_test_base.h"

#include <string>

#include "base/path_service.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"

using extensions::Extension;

ExtensionSettingsTestBase::ExtensionSettingsTestBase()
    : test_data_dir_(base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
                         .AppendASCII("extensions")) {}

ExtensionSettingsTestBase::~ExtensionSettingsTestBase() = default;

void ExtensionSettingsTestBase::InstallGoodExtension() {
  EXPECT_TRUE(InstallExtension(test_data_dir_.AppendASCII("good.crx")));
}

void ExtensionSettingsTestBase::InstallErrorsExtension() {
  EXPECT_TRUE(
      InstallExtension(test_data_dir_.AppendASCII("error_console")
                           .AppendASCII("runtime_and_manifest_errors")));
  EXPECT_TRUE(InstallExtension(test_data_dir_.AppendASCII("error_console")
                                   .AppendASCII("deep_stack_trace")));
}

void ExtensionSettingsTestBase::InstallSharedModule() {
  base::FilePath shared_module_path =
      test_data_dir_.AppendASCII("api_test").AppendASCII("shared_module");
  EXPECT_TRUE(InstallExtension(shared_module_path.AppendASCII("shared")));
  EXPECT_TRUE(InstallExtension(shared_module_path.AppendASCII("import_pass")));
}

void ExtensionSettingsTestBase::InstallPackagedApp() {
  EXPECT_TRUE(InstallExtension(test_data_dir_.AppendASCII("packaged_app")));
}

void ExtensionSettingsTestBase::InstallHostedApp() {
  EXPECT_TRUE(InstallExtension(test_data_dir_.AppendASCII("hosted_app")));
}

void ExtensionSettingsTestBase::InstallPlatformApp() {
  EXPECT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("minimal")));
}

const extensions::Extension*
ExtensionSettingsTestBase::InstallExtensionWithInPageOptions() {
  const extensions::Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("options_page_in_view"));
  EXPECT_TRUE(extension);
  return extension;
}

void ExtensionSettingsTestBase::SetAutoConfirmUninstall() {
  uninstall_auto_confirm_ =
      std::make_unique<extensions::ScopedTestDialogAutoConfirm>(
          extensions::ScopedTestDialogAutoConfirm::ACCEPT);
}

void ExtensionSettingsTestBase::SetDevModeEnabled(bool enabled) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kExtensionsUIDeveloperMode, enabled);
}

void ExtensionSettingsTestBase::SetSilenceDeprecatedManifestVersionWarnings(
    bool silence) {
  Extension::set_silence_deprecated_manifest_version_warnings_for_testing(
      silence);
}

const Extension* ExtensionSettingsTestBase::InstallExtension(
    const base::FilePath& path) {
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  loader.set_ignore_manifest_warnings(true);
  return loader.LoadExtension(path).get();
}
