// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_LOCAL_NTP_TEST_UTILS_H_
#define CHROME_BROWSER_UI_SEARCH_LOCAL_NTP_TEST_UTILS_H_

#include <string>

class Browser;
class GURL;
class Profile;

namespace content {
class DOMMessageQueue;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace local_ntp_test_utils {

// Tests that dark mode styling is properly applied to the local NTP.
// class BaseDarkModeTest;

content::WebContents* OpenNewTab(Browser* browser, const GURL& url);

// Executes a script on the NTP, verifies it executes successfully, and waits
// until the NTP tiles are reloaded. Note that simply waiting for the script
// execution to complete is not enough, since the MV iframe receives the tiles
// asynchronously.
void ExecuteScriptOnNTPAndWaitUntilLoaded(content::RenderFrameHost* host,
                                          const std::string& script);

// Waits until the NTP tiles are loaded after a |delay|. |msg_queue| must be
// initialized with |active_tab| before calling this function, otherwise we may
// miss the 'loaded' message.
void WaitUntilTilesLoaded(content::WebContents* active_tab,
                          content::DOMMessageQueue* msg_queue,
                          int delay);

// Switches the browser language to French, and returns true iff successful.
bool SwitchBrowserLanguageToFrench();

void SetUserSelectedDefaultSearchProvider(Profile* profile,
                                          const std::string& base_url,
                                          const std::string& ntp_url);

// Get the URL that WebContents->GetVisibleURL() will return after navigating to
// chrome://newtab/.  While this should typically be chrome://newtab/, in a test
// environment where there is no network connection, it may be
// chrome-search://local-ntp/local-ntp.html.
GURL GetFinalNtpUrl(Profile* profile);

}  // namespace local_ntp_test_utils

#endif  // CHROME_BROWSER_UI_SEARCH_LOCAL_NTP_TEST_UTILS_H_
