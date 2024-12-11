// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LOG_WEB_UI_URL_H_
#define CHROME_BROWSER_UI_WEBUI_LOG_WEB_UI_URL_H_

#include <variant>

class GURL;

namespace content {
class WebUI;
}

namespace webui {

// Name of histogram that WebUI URLs are logged to when the WebUI object is
// created.
extern const char kWebUICreatedForUrl[];

// Name of histogram that WebUI URLs are logged to when the WebUI is shown.
extern const char kWebUIShownUrl[];

// Called when a WebUI object is created. Only internal (e.g. chrome://) URLs
// are logged. Returns whether the URL was actually logged. This is used to
// collect WebUI usage data.
bool LogWebUICreated(const GURL& web_ui_url);

// Called when a WebUI object is shown. Only internal (e.g. chrome://) URLs
// are logged. Returns whether the URL was actually logged. This is used to
// collect WebUI usage data. Preloaded WebUIs may be created but not shown.
bool LogWebUIShown(const GURL& web_ui_url);

// Logs when a WebUI is created. Only internal (e.g. chrome://) URLs
// are logged. This calls LogWebUICreated() and LogWebUIShown() if actually
// shown. The url variant is used for WebUIs that don't have a WebUI object
// (crbug.com/40089364).
void LogWebUIUsage(std::variant<content::WebUI*, GURL> webui_variant);

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_LOG_WEB_UI_URL_H_
