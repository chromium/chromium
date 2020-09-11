// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_CHROME_CONTENT_BROWSER_CLIENT_TAB_STRIP_PART_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_CHROME_CONTENT_BROWSER_CLIENT_TAB_STRIP_PART_H_

#include "base/macros.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

class ChromeContentBrowserClientTabStripPart
    : public ChromeContentBrowserClientParts {
 public:
  ChromeContentBrowserClientTabStripPart();
  ~ChromeContentBrowserClientTabStripPart() override;

  // ChromeContentBrowserClientParts:
  void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                           blink::web_pref::WebPreferences* web_prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClientTabStripPart);
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_CHROME_CONTENT_BROWSER_CLIENT_TAB_STRIP_PART_H_
