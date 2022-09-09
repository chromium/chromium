// Copyright 2019 The Chromium Authors
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
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/ui_base_switches.h"

class ChromeContentBrowserClientTabStripPartTest : public InProcessBrowserTest {
 public:
  std::unique_ptr<content::WebContents> CreateTabStripWebContents() {
    std::unique_ptr<content::WebContents> webui_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    webui_contents->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(
            GURL(chrome::kChromeUITabStripURL)));
    return webui_contents;
  }

 protected:
  std::unique_ptr<content::WebContents> webui_contents_;
};

IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientTabStripPartTest,
                       TabStripHasDefaultFontSizes) {
  const blink::web_pref::WebPreferences default_prefs;
  const int kDefaultFontSize = default_prefs.default_font_size;
  const int kDefaultFixedFontSize = default_prefs.default_fixed_font_size;
  const int kDefaultMinimumFontSize = default_prefs.minimum_font_size;
  const int kDefaultMinimumLogicalFontSize =
      default_prefs.minimum_logical_font_size;

  blink::web_pref::WebPreferences preexisting_tab_strip_prefs =
      CreateTabStripWebContents()->GetOrCreateWebPreferences();

  Profile* profile = browser()->profile();
  PrefService* profile_prefs = profile->GetPrefs();
  profile_prefs->SetInteger(prefs::kWebKitDefaultFontSize,
                            kDefaultFontSize + 1);
  profile_prefs->SetInteger(prefs::kWebKitDefaultFixedFontSize,
                            kDefaultFixedFontSize + 2);
  profile_prefs->SetInteger(prefs::kWebKitMinimumFontSize,
                            kDefaultMinimumFontSize + 3);
  profile_prefs->SetInteger(prefs::kWebKitMinimumLogicalFontSize,
                            kDefaultMinimumLogicalFontSize + 4);

  EXPECT_EQ(kDefaultFontSize, preexisting_tab_strip_prefs.default_font_size);
  EXPECT_EQ(kDefaultFixedFontSize,
            preexisting_tab_strip_prefs.default_fixed_font_size);
  EXPECT_EQ(kDefaultMinimumFontSize,
            preexisting_tab_strip_prefs.minimum_font_size);
  EXPECT_EQ(kDefaultMinimumLogicalFontSize,
            preexisting_tab_strip_prefs.minimum_logical_font_size);

  blink::web_pref::WebPreferences new_tab_strip_prefs =
      CreateTabStripWebContents()->GetOrCreateWebPreferences();
  EXPECT_EQ(kDefaultFontSize, new_tab_strip_prefs.default_font_size);
  EXPECT_EQ(kDefaultFixedFontSize, new_tab_strip_prefs.default_fixed_font_size);
  EXPECT_EQ(kDefaultMinimumFontSize, new_tab_strip_prefs.minimum_font_size);
  EXPECT_EQ(kDefaultMinimumLogicalFontSize,
            new_tab_strip_prefs.minimum_logical_font_size);
}
