// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaBrowserTest CrComponentsColorChangeListenerTest;
IN_PROC_BROWSER_TEST_F(CrComponentsColorChangeListenerTest, All) {
  RunTest("cr_components/color_change_listener_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrComponentsManagedFootnoteTest;
IN_PROC_BROWSER_TEST_F(CrComponentsManagedFootnoteTest, All) {
  // Loaded from chrome://settings because it needs access to chrome.send().
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_components/managed_footnote_test.js", "mocha.run()");
}

#if BUILDFLAG(USE_NSS_CERTS)
typedef WebUIMochaBrowserTest CrComponentsCertificateManagerTest;
IN_PROC_BROWSER_TEST_F(CrComponentsCertificateManagerTest, All) {
  // Loaded from a settings URL so that localized strings are present.
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_components/certificate_manager_test.js", "mocha.run()");
}
#endif  // BUILDFLAG(USE_NSS_CERTS)

#if BUILDFLAG(USE_NSS_CERTS) && BUILDFLAG(IS_CHROMEOS)
typedef WebUIMochaBrowserTest CrComponentsCertificateManagerProvisioningTest;
IN_PROC_BROWSER_TEST_F(CrComponentsCertificateManagerProvisioningTest, All) {
  // Loaded from a settings URL so that localized strings are present.
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_components/certificate_manager_provisioning_test.js",
          "mocha.run()");
}
#endif  // BUILDFLAG(USE_NSS_CERTS) && BUILDFLAG(IS_CHROMEOS)

typedef WebUIMochaBrowserTest CrComponentsManagedDialogTest;
IN_PROC_BROWSER_TEST_F(CrComponentsManagedDialogTest, All) {
  RunTest("cr_components/managed_dialog_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrComponentsLocalizedLinkTest;
IN_PROC_BROWSER_TEST_F(CrComponentsLocalizedLinkTest, All) {
  RunTest("cr_components/localized_link_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrComponentsAppManagementPermissionItemTest;
IN_PROC_BROWSER_TEST_F(CrComponentsAppManagementPermissionItemTest, All) {
  RunTest("cr_components/app_management/permission_item_test.js",
          "mocha.run()");
}

typedef WebUIMochaBrowserTest CrComponentsAppManagementFileHandlingItemTest;
IN_PROC_BROWSER_TEST_F(CrComponentsAppManagementFileHandlingItemTest, All) {
  RunTest("cr_components/app_management/file_handling_item_test.js",
          "mocha.run()");
}

typedef WebUIMochaBrowserTest CrComponentsAppManagementWindowModeTest;
IN_PROC_BROWSER_TEST_F(CrComponentsAppManagementWindowModeTest, All) {
  RunTest("cr_components/app_management/window_mode_item_test.js",
          "mocha.run()");
}

typedef WebUIMochaBrowserTest CrComponentsAppManagementUninstallButtonTest;
IN_PROC_BROWSER_TEST_F(CrComponentsAppManagementUninstallButtonTest, All) {
  RunTest("cr_components/app_management/uninstall_button_test.js",
          "mocha.run()");
}

typedef WebUIMochaBrowserTest CrComponentsSettingsPrefsTest;
IN_PROC_BROWSER_TEST_F(CrComponentsSettingsPrefsTest, All) {
  // Preload a settings URL, so that the test can access settingsPrivate.
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_components/settings_prefs_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrComponentsSettingsPrefUtilsTest;
IN_PROC_BROWSER_TEST_F(CrComponentsSettingsPrefUtilsTest, All) {
  // Preload a settings URL, so that the test can access settingsPrivate.
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_components/settings_pref_util_test.js", "mocha.run()");
}
