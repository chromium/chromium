// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_CHROME_CONTENT_BROWSER_CLIENT_TAB_STRIP_PART_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_CHROME_CONTENT_BROWSER_CLIENT_TAB_STRIP_PART_H_

#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

class ChromeContentBrowserClientTabStripPart
    : public ChromeContentBrowserClientParts {
 public:
  ChromeContentBrowserClientTabStripPart();

  ChromeContentBrowserClientTabStripPart(
      const ChromeContentBrowserClientTabStripPart&) = delete;
  ChromeContentBrowserClientTabStripPart& operator=(
      const ChromeContentBrowserClientTabStripPart&) = delete;

  ~ChromeContentBrowserClientTabStripPart() override;

  // ChromeContentBrowserClientParts:
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* web_prefs) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_CHROME_CONTENT_BROWSER_CLIENT_TAB_STRIP_PART_H_
