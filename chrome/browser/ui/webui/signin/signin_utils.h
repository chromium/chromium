// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_H_

#include <string>

#include "base/values.h"

class Browser;

namespace content {
class RenderFrameHost;
class WebContents;
class WebUI;
}

namespace signin {

// Gets a webview within an auth page that has the specified parent frame name
// (i.e. <webview name="foobar"></webview>).
content::RenderFrameHost* GetAuthFrame(content::WebContents* web_contents,
                                       const std::string& parent_frame_name);

content::WebContents* GetAuthFrameWebContents(
    content::WebContents* web_contents,
    const std::string& parent_frame_name);

// Gets the browser containing the web UI; if none is found, returns the last
// active browser for web UI's profile.
Browser* GetDesktopBrowser(content::WebUI* web_ui);

// Sets the height of the WebUI modal dialog after its initialization. This is
// needed to better accomodate different locales' text heights.
void SetInitializedModalHeight(Browser* browser,
                               content::WebUI* web_ui,
                               const base::ListValue* args);

}  // namespace signin

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_H_
