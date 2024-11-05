// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LOG_WEB_UI_URL_H_
#define CHROME_BROWSER_UI_WEBUI_LOG_WEB_UI_URL_H_

class GURL;

namespace webui {

// Name of histogram that WebUI URLs are logged to when the WebUI object is
// created.
extern const char kWebUICreatedForUrl[];

// Name of histogram that WebUI URLs are logged to when the WebUI is shown.
extern const char kWebUIShownUrl[];

// Called when WebUI objects are created. Only internal (e.g. chrome://) URLs
// are logged. Note that a WebUI can be created but never shown, which will
// also be logged by this function. Returns whether the URL was actually logged.
// This is used to collect WebUI usage data.
bool LogWebUICreated(const GURL& web_ui_url);

// Called when a WebUI completes the first non-empty paint. Only internal
// (e.g. chrome://) URLs are logged. Returns whether the URL was actually
// logged. This is used to collect WebUI usage data.
bool LogWebUIShown(const GURL& web_ui_url);

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_LOG_WEB_UI_URL_H_
