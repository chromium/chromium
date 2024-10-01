// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/content_settings/core/common/features.h"
#include "content/public/test/browser_test.h"

class SettingsFocusTest : public WebUIMochaFocusTest {
 protected:
  SettingsFocusTest() { set_test_loader_host(chrome::kChromeUISettingsHost); }
};

IN_PROC_BROWSER_TEST_F(SettingsFocusTest, AnimatedPages) {
  RunTest("settings/settings_animated_pages_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsFocusTest, AutofillSectionFocus) {
  RunTest("settings/autofill_section_focus_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsFocusTest, PaymentsSectionInteractive) {
  RunTest("settings/payments_section_interactive_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsFocusTest, PaymentsSectionFocus) {
  RunTest("settings/payments_section_focus_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsFocusTest, SyncPage) {
  RunTest("settings/people_page_sync_page_interactive_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsFocusTest, SecureDns) {
  RunTest("settings/secure_dns_interactive_test.js", "mocha.run()");
}

// Times out on Mac. See https://crbug.com/1060981.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SettingsUIToolbarAndDrawer DISABLED_SettingsUIToolbarAndDrawer
#else
#define MAYBE_SettingsUIToolbarAndDrawer SettingsUIToolbarAndDrawer
#endif
IN_PROC_BROWSER_TEST_F(SettingsFocusTest, MAYBE_SettingsUIToolbarAndDrawer) {
  RunTest("settings/settings_ui_test.js",
          "runMochaSuite('SettingsUIToolbarAndDrawer')");
}

// Times out on Mac. See https://crbug.com/1060981.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SettingsUISearch DISABLED_SettingsUISearch
#else
#define MAYBE_SettingsUISearch SettingsUISearch
#endif
IN_PROC_BROWSER_TEST_F(SettingsFocusTest, MAYBE_SettingsUISearch) {
  RunTest("settings/settings_ui_test.js", "runMochaSuite('SettingsUISearch')");
}

IN_PROC_BROWSER_TEST_F(SettingsFocusTest, Menu) {
  RunTest("settings/settings_menu_interactive_ui_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsFocusTest, ReviewNotificationPermissionsFocus) {
  RunTest("settings/review_notification_permissions_interactive_ui_test.js",
          "mocha.run()");
}

class SettingsUnusedSitePermissionsFocusTest : public SettingsFocusTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      content_settings::features::kSafetyCheckUnusedSitePermissions};
};

IN_PROC_BROWSER_TEST_F(SettingsUnusedSitePermissionsFocusTest, All) {
  RunTest("settings/unused_site_permissions_interactive_ui_test.js",
          "mocha.run()");
}
