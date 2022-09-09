// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/chrome_content_browser_client_tab_strip_part.h"

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

ChromeContentBrowserClientTabStripPart::
    ChromeContentBrowserClientTabStripPart() = default;
ChromeContentBrowserClientTabStripPart::
    ~ChromeContentBrowserClientTabStripPart() = default;

void ChromeContentBrowserClientTabStripPart::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* web_prefs) {
  if (!web_contents)
    return;

  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  GURL url = entry ? entry->GetURL() : GURL();

  if (url.host_piece() != chrome::kChromeUITabStripHost) {
    return;
  }

  blink::web_pref::WebPreferences default_prefs;
  web_prefs->default_font_size = default_prefs.default_font_size;
  web_prefs->default_fixed_font_size = default_prefs.default_fixed_font_size;
  web_prefs->minimum_font_size = default_prefs.minimum_font_size;
  web_prefs->minimum_logical_font_size =
      default_prefs.minimum_logical_font_size;
  web_prefs->touch_drag_drop_enabled = true;
  web_prefs->touch_dragend_context_menu = true;
}
