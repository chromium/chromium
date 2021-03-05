// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_content_browser_client_webui_part.h"

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

ChromeContentBrowserClientWebUIPart::ChromeContentBrowserClientWebUIPart() =
    default;
ChromeContentBrowserClientWebUIPart::~ChromeContentBrowserClientWebUIPart() =
    default;

void ChromeContentBrowserClientWebUIPart::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* web_prefs) {
  if (!web_contents)
    return;

  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  GURL url = entry ? entry->GetURL() : GURL();

  if (!url.SchemeIs(content::kChromeUIScheme)) {
    return;
  }

  // Prevent font size preferences from affecting chrome:// WebUI pages.
  blink::web_pref::WebPreferences default_prefs;
  web_prefs->default_font_size = default_prefs.default_font_size;
  web_prefs->default_fixed_font_size = default_prefs.default_fixed_font_size;
  web_prefs->minimum_font_size = default_prefs.minimum_font_size;
  web_prefs->minimum_logical_font_size =
      default_prefs.minimum_logical_font_size;

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  if (url.host_piece() == chrome::kChromeUITabStripHost) {
    web_prefs->touch_drag_drop_enabled = true;
    web_prefs->touch_dragend_context_menu = true;
  }
#endif
}
