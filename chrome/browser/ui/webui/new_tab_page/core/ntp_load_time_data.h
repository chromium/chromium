// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_CORE_NTP_LOAD_TIME_DATA_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_CORE_NTP_LOAD_TIME_DATA_H_

#include <vector>

#include "base/values.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace ntp {

// Registers the URL data sources (`SanitizedImageSource`, `FaviconSource`,
// `UntrustedSource`, `ThemeSource`) that New Tab Page documents and their
// embedded `chrome-untrusted://` frames load from.
void AddDataSources(Profile* profile);

// Auto-grants 3P cookies to the One Google Bar embedded in
// chrome-untrusted://new-tab-page. No-op on ChromeOS (b/314326552).
void AddThirdPartyCookieGrants(Profile* profile);

// Returns whether the action chips are shown on the NTP.
bool AreActionChipsEnabled(Profile* profile);

// Builds the static New Tab Page `loadTimeData` dictionary for `profile`
// (localized strings, feature flags, and searchbox/composebox parameters).
// Navigation-dependent and theme-dependent entries are built separately by
// `BuildNavigationLoadTimeData()`, `BuildBackgroundColorLoadTimeData()`, and
// `BuildCustomBackgroundImageLoadTimeData()`.
base::DictValue BuildLoadTimeData(Profile* profile,
                                  bool session_allows_drag_and_drop);

// Builds the per-navigation `loadTimeData` entries (`"modulesEnabled"` and
// `"browserPromoType"`).
base::DictValue BuildNavigationLoadTimeData(
    Profile* profile,
    content::WebContents* web_contents,
    const std::vector<ModuleIdDetail>& module_id_details);

// Builds the `"backgroundColor"` `loadTimeData` entry for `web_contents`'s
// current `ui::ColorProvider`.
base::DictValue BuildBackgroundColorLoadTimeData(
    const content::WebContents* web_contents);

// Builds the `"backgroundImageUrl"` `loadTimeData` entry for `profile`'s
// `NtpCustomBackgroundService`.
base::DictValue BuildCustomBackgroundImageLoadTimeData(Profile* profile);

}  // namespace ntp

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_CORE_NTP_LOAD_TIME_DATA_H_
