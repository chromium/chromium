// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/history_clusters/core/features.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaBrowserTest CrComponentsTest;

#if BUILDFLAG(USE_NSS_CERTS)
IN_PROC_BROWSER_TEST_F(CrComponentsTest, CertificateManager) {
  // Loaded from a settings URL so that localized strings are present.
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_components/certificate_manager_test.js", "mocha.run()");
}
#endif  // BUILDFLAG(USE_NSS_CERTS)

#if BUILDFLAG(USE_NSS_CERTS) && BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(CrComponentsTest, CertificateManagerProvisioning) {
  // Loaded from a settings URL so that localized strings are present.
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_components/certificate_manager_provisioning_test.js",
          "mocha.run()");
}
#endif  // BUILDFLAG(USE_NSS_CERTS) && BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(CrComponentsTest, ColorChangeListener) {
  RunTest("cr_components/color_change_listener_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, CustomizeColorSchemeMode) {
  set_test_loader_host(chrome::kChromeUICustomizeChromeSidePanelHost);
  RunTest("cr_components/customize_color_scheme_mode_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, CustomizeThemes) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/customize_themes_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, HelpBubbleMixin) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/help_bubble_mixin_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, HelpBubble) {
  set_test_loader_host(chrome::kChromeUINewTabPageHost);
  RunTest("cr_components/help_bubble_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, ManagedDialog) {
  RunTest("cr_components/managed_dialog_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, ManagedFootnote) {
  // Loaded from chrome://settings because it needs access to chrome.send().
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_components/managed_footnote_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, LocalizedLink) {
  RunTest("cr_components/localized_link_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, SettingsPrefs) {
  // Preload a settings URL, so that the test can access settingsPrivate.
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_components/settings_prefs_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsTest, SettingsPrefUtils) {
  // Preload a settings URL, so that the test can access settingsPrivate.
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("cr_components/settings_pref_util_test.js", "mocha.run()");
}

typedef WebUIMochaBrowserTest CrComponentsAppManagementTest;
IN_PROC_BROWSER_TEST_F(CrComponentsAppManagementTest, PermissionItem) {
  RunTest("cr_components/app_management/permission_item_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsAppManagementTest, FileHandlingItem) {
  RunTest("cr_components/app_management/file_handling_item_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsAppManagementTest, WindowModeItem) {
  RunTest("cr_components/app_management/window_mode_item_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsAppManagementTest, UninstallButton) {
  RunTest("cr_components/app_management/uninstall_button_test.js",
          "mocha.run()");
}

class CrComponentsHistoryClustersTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsHistoryClustersTest() {
    scoped_feature_list_.InitAndEnableFeature(
        history_clusters::internal::kJourneysImages);
    set_test_loader_host(chrome::kChromeUIHistoryHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrComponentsHistoryClustersTest, All) {
  RunTest("cr_components/history_clusters_test.js", "mocha.run()");
}

class CrComponentsMostVisitedTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsMostVisitedTest() {
    set_test_loader_host(chrome::kChromeUINewTabPageHost);
  }
};

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, General) {
  RunTest("cr_components/most_visited_test.js", "runMochaSuite('General');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, Layouts) {
  RunTest("cr_components/most_visited_test.js", "runMochaSuite('Layouts');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, ReflowLayouts) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('Reflow Layouts');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, LoggingAndUpdates) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('LoggingAndUpdates');");
}

// crbug.com/1226996
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_Modification DISABLED_Modification
#else
#define MAYBE_Modification Modification
#endif
IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, MAYBE_Modification) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('Modification');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, DragAndDrop) {
  RunTest("cr_components/most_visited_test.js",
          "runMochaSuite('DragAndDrop');");
}

IN_PROC_BROWSER_TEST_F(CrComponentsMostVisitedTest, Theming) {
  RunTest("cr_components/most_visited_test.js", "runMochaSuite('Theming');");
}
