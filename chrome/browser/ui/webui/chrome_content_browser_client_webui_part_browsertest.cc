// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/ui_base_switches.h"

namespace {
const blink::web_pref::WebPreferences GetCustomFontPrefs() {
  blink::web_pref::WebPreferences prefs;
  prefs.default_font_size += 1;
  prefs.default_fixed_font_size += 2;
  prefs.minimum_font_size += 3;
  prefs.minimum_logical_font_size += 4;
  return prefs;
}
}  // namespace

class ChromeContentBrowserClientWebUiPartTest : public InProcessBrowserTest {
 protected:
  // Registers and returns custom preferences for font size, which differ from
  // the default preferences.
  const blink::web_pref::WebPreferences RegisterCustomFontPrefs() {
    const blink::web_pref::WebPreferences prefs = GetCustomFontPrefs();
    Profile* profile = browser()->profile();
    PrefService* profile_prefs = profile->GetPrefs();
    profile_prefs->SetInteger(prefs::kWebKitDefaultFontSize,
                              prefs.default_font_size);
    profile_prefs->SetInteger(prefs::kWebKitDefaultFixedFontSize,
                              prefs.default_fixed_font_size);
    profile_prefs->SetInteger(prefs::kWebKitMinimumFontSize,
                              prefs.minimum_font_size);
    profile_prefs->SetInteger(prefs::kWebKitMinimumLogicalFontSize,
                              prefs.minimum_logical_font_size);
    return prefs;
  }

  void AssertFontPrefsEqual(const blink::web_pref::WebPreferences& expected,
                            blink::web_pref::WebPreferences& actual) {
    EXPECT_EQ(expected.default_font_size, actual.default_font_size);
    EXPECT_EQ(expected.default_fixed_font_size, actual.default_fixed_font_size);
    EXPECT_EQ(expected.minimum_font_size, actual.minimum_font_size);
    EXPECT_EQ(expected.minimum_logical_font_size,
              actual.minimum_logical_font_size);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientWebUiPartTest,
                       HasDefaultFontSizes) {
  RegisterCustomFontPrefs();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIExtensionsURL)));
  const blink::web_pref::WebPreferences default_prefs;
  blink::web_pref::WebPreferences preexisting_prefs =
      browser()
          ->tab_strip_model()
          ->GetActiveWebContents()
          ->GetOrCreateWebPreferences();
  AssertFontPrefsEqual(default_prefs, preexisting_prefs);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIExtensionsURL)));
  blink::web_pref::WebPreferences new_prefs = browser()
                                                  ->tab_strip_model()
                                                  ->GetActiveWebContents()
                                                  ->GetOrCreateWebPreferences();
  AssertFontPrefsEqual(default_prefs, new_prefs);
}

IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientWebUiPartTest,
                       NavigateFromWebUiToNonWebUi) {
  const blink::web_pref::WebPreferences custom_prefs =
      RegisterCustomFontPrefs();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIExtensionsURL)));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const GURL url = web_contents->GetController().GetVisibleEntry()->GetURL();
  ASSERT_TRUE(url.SchemeIs(content::kChromeUIScheme));

  // Assert that WebUi uses the default prefs, and not the user's defined custom
  // prefs.
  const blink::web_pref::WebPreferences default_prefs;
  blink::web_pref::WebPreferences active_webui_prefs =
      web_contents->GetOrCreateWebPreferences();
  AssertFontPrefsEqual(default_prefs, active_webui_prefs);

  // Assert that transitioning from a WebUi URL to a non-WebUi URL in the same
  // tab, uses the user's defined custom prefs.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  const GURL url2 = web_contents->GetController().GetVisibleEntry()->GetURL();
  ASSERT_FALSE(url2.SchemeIs(content::kChromeUIScheme));
  blink::web_pref::WebPreferences active_non_webui_prefs =
      web_contents->GetOrCreateWebPreferences();
  AssertFontPrefsEqual(custom_prefs, active_non_webui_prefs);
}

IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientWebUiPartTest,
                       NavigateFromNonWebUiToWebUi) {
  const blink::web_pref::WebPreferences custom_prefs =
      RegisterCustomFontPrefs();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  const GURL url2 = web_contents->GetController().GetVisibleEntry()->GetURL();
  ASSERT_FALSE(url2.SchemeIs(content::kChromeUIScheme));

  // Assert that non-WebUi uses the user's defined custom prefs.
  blink::web_pref::WebPreferences active_non_webui_prefs =
      web_contents->GetOrCreateWebPreferences();
  AssertFontPrefsEqual(custom_prefs, active_non_webui_prefs);

  // Assert that transitioning from a non-WebUi URL to a WebUi URL in the same
  // tab, uses the default prefs.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIExtensionsURL)));
  const GURL url = web_contents->GetController().GetVisibleEntry()->GetURL();
  ASSERT_TRUE(url.SchemeIs(content::kChromeUIScheme));
  const blink::web_pref::WebPreferences default_prefs;
  blink::web_pref::WebPreferences active_webui_prefs =
      web_contents->GetOrCreateWebPreferences();
  AssertFontPrefsEqual(default_prefs, active_webui_prefs);
}
