// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_content_browser_client_webui_part.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace {

// Returns whether any prefs were changed.
bool CopyFontPrefs(const blink::web_pref::WebPreferences& source,
                   blink::web_pref::WebPreferences* destination) {
  bool changed = false;
  changed |= destination->default_font_size != source.default_font_size;
  changed |=
      destination->default_fixed_font_size != source.default_fixed_font_size;
  changed |= destination->minimum_font_size != source.minimum_font_size;
  changed |= destination->minimum_logical_font_size !=
             source.minimum_logical_font_size;

  if (!changed) {
    return false;
  }

  destination->default_font_size = source.default_font_size;
  destination->default_fixed_font_size = source.default_fixed_font_size;
  destination->minimum_font_size = source.minimum_font_size;
  destination->minimum_logical_font_size = source.minimum_logical_font_size;

  return true;
}

// Returns the visible URL or GURL() if unavailable.
GURL GetVisibleURL(content::WebContents* web_contents) {
  if (!web_contents) {
    return GURL();
  }

  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  return entry ? entry->GetURL() : GURL();
}

}  // namespace

ChromeContentBrowserClientWebUiPart::ChromeContentBrowserClientWebUiPart() =
    default;
ChromeContentBrowserClientWebUiPart::~ChromeContentBrowserClientWebUiPart() =
    default;

void ChromeContentBrowserClientWebUiPart::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* web_prefs) {
  // This logic is invoked at startup, and anytime the default prefs change.
  GURL url = GetVisibleURL(web_contents);

  if (!url.SchemeIs(content::kChromeUIScheme)) {
    return;
  }

  // Use default font sizes for WebUi.
  blink::web_pref::WebPreferences default_prefs;
  CopyFontPrefs(/*source=*/default_prefs, /*destination=*/web_prefs);

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  // Set some non-font prefs for webui tabstrip. The tabstrip renderer is never
  // navigated to or from, so we don't need to replicate this logic in
  // OverrideWebPreferencesAfterNavigation.
  if (url.host_piece() == chrome::kChromeUITabStripHost) {
    web_prefs->touch_drag_drop_enabled = true;
    web_prefs->touch_dragend_context_menu = true;
  }
#endif
}

bool ChromeContentBrowserClientWebUiPart::OverrideWebPreferencesAfterNavigation(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* web_prefs) {
  // This logic is invoked once on each navigation.

  GURL url = GetVisibleURL(web_contents);
  if (!url.is_valid()) {
    return false;
  }

  // Extensions are handled by ChromeContentBrowserClientExtensionsPart.
  const GURL& site_url =
      web_contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL();
  if (site_url.SchemeIs(extensions::kExtensionScheme)) {
    return false;
  }

  blink::web_pref::WebPreferences web_prefs_source;
  if (url.SchemeIs(content::kChromeUIScheme)) {
    // Use default prefs for WebUi. Not further modifications necessary for
    // web_prefs_source.
  } else {
    // Use profile prefs for normal websites.
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    PrefService* prefs = profile->GetPrefs();
    web_prefs_source.default_font_size =
        prefs->GetInteger(prefs::kWebKitDefaultFontSize);
    web_prefs_source.default_fixed_font_size =
        prefs->GetInteger(prefs::kWebKitDefaultFixedFontSize);
    web_prefs_source.minimum_font_size =
        prefs->GetInteger(prefs::kWebKitMinimumFontSize);
    web_prefs_source.minimum_logical_font_size =
        prefs->GetInteger(prefs::kWebKitMinimumLogicalFontSize);
  }
  return CopyFontPrefs(web_prefs_source, web_prefs);
}
