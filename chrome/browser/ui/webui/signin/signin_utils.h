// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_H_

#include <string>

#include "base/callback.h"
#include "base/values.h"

class Browser;

namespace content {
class RenderFrameHost;
class WebContents;
class WebUI;
}

namespace extensions {
class WebViewGuest;
}

namespace signin {

// User choice when signing in.
// Used for UMA histograms, Hence, constants should never be deleted or
// reordered, and  new constants should only be appended at the end.
// Keep this in sync with SigninChoice in histograms.xml.
enum SigninChoice {
  SIGNIN_CHOICE_CANCEL = 0,       // Signin is cancelled.
  SIGNIN_CHOICE_CONTINUE = 1,     // Signin continues in the current profile.
  SIGNIN_CHOICE_NEW_PROFILE = 2,  // Signin continues in a new profile.
  // SIGNIN_CHOICE_SIZE should always be last.
  SIGNIN_CHOICE_SIZE,
};

using SigninChoiceCallback = base::OnceCallback<void(SigninChoice)>;

// Gets a webview within an auth page that has the specified parent frame name
// (i.e. <webview name="foobar"></webview>).
content::RenderFrameHost* GetAuthFrame(content::WebContents* web_contents,
                                       const std::string& parent_frame_name);

extensions::WebViewGuest* GetAuthWebViewGuest(
    content::WebContents* web_contents,
    const std::string& parent_frame_name);

// Gets the browser containing the web UI; if none is found, returns the last
// active browser for web UI's profile.
Browser* GetDesktopBrowser(content::WebUI* web_ui);

// Sets the height of the WebUI modal dialog after its initialization. This is
// needed to better accomodate different locales' text heights.
void SetInitializedModalHeight(Browser* browser,
                               content::WebUI* web_ui,
                               const base::Value::List& args);

}  // namespace signin

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_H_
