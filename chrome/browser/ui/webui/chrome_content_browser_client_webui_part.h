// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_CONTENT_BROWSER_CLIENT_WEBUI_PART_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_CONTENT_BROWSER_CLIENT_WEBUI_PART_H_

#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

class ChromeContentBrowserClientWebUiPart
    : public ChromeContentBrowserClientParts {
 public:
  ChromeContentBrowserClientWebUiPart();

  ChromeContentBrowserClientWebUiPart(
      const ChromeContentBrowserClientWebUiPart&) = delete;
  ChromeContentBrowserClientWebUiPart& operator=(
      const ChromeContentBrowserClientWebUiPart&) = delete;

  ~ChromeContentBrowserClientWebUiPart() override;

  // ChromeContentBrowserClientParts:
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* web_prefs) override;
  bool OverrideWebPreferencesAfterNavigation(
      content::WebContents* web_contents,
      blink::web_pref::WebPreferences* web_prefs) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_CONTENT_BROWSER_CLIENT_WEBUI_PART_H_
