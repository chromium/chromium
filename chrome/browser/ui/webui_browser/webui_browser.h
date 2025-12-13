// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_H_

namespace content {
class WebContents;
}  // namespace content

namespace webui_browser {

bool IsWebUIBrowserEnabled();

bool IsBrowserUIWebContents(content::WebContents* web_contents);

}  // namespace webui_browser

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_H_
