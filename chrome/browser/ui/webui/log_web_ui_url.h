// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LOG_WEB_UI_URL_H_
#define CHROME_BROWSER_UI_WEBUI_LOG_WEB_UI_URL_H_

class GURL;

namespace webui {

// Name of histogram that WebUI URLs are logged to.
extern const char kWebUICreatedForUrl[];

// Called when WebUI objects are created to get aggregate usage data (i.e. is
// chrome://history used more than chrome://help?). Only internal (e.g.
// chrome://) URLs are logged. Returns whether the URL was actually logged.
bool LogWebUIUrl(const GURL& web_ui_url);

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_LOG_WEB_UI_URL_H_
