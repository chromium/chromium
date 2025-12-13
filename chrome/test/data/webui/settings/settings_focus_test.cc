// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/content_settings/core/common/features.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#endif

class SettingsFocusTest : public WebUIMochaFocusTest {
 protected:
  SettingsFocusTest() { set_test_loader_host(chrome::kChromeUISettingsHost); }
};

IN_PROC_BROWSER_TEST_F(SettingsFocusTest, AutofillSectionFocus) {
  RunTest("settings/autofill_section_focus_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsFocusTest, PaymentsSectionInteractive) {
  RunTest("settings/payments_section_interactive_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsFocusTest, PaymentsSectionFocus) {
  RunTest("settings/payments_section_focus_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(SettingsFocusTest, SettingsViewMixin) {
  RunTest("settings/settings_view_mixin_test.js", "mocha.run()");
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

#if BUILDFLAG(ENABLE_GLIC)
class SettingsGlicSubpageFocusTest : public SettingsFocusTest {
 public:
  SettingsGlicSubpageFocusTest() = default;

 private:
  glic::GlicTestEnvironment glic_test_env_;
};

// TODO(crbug.com/424864547): Investigate flakiness and enable on Mac64 and
// Win64.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_GlicSubpageFocus DISABLED_GlicSubpageFocus
#else
#define MAYBE_GlicSubpageFocus GlicSubpageFocus
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(SettingsGlicSubpageFocusTest, MAYBE_GlicSubpageFocus) {
  RunTest("settings/glic_subpage_focus_test.js", "mocha.run()");
}
#endif
