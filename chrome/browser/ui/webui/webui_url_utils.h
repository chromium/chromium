// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_URL_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_URL_UTILS_H_

class GURL;

// Returns true if the WebUIConfig registered for `url` specifies
// `chrome::kChromeUITopChromeDomain` as its process lock host, i.e. `url` is a
// trusted `chrome://` WebUI that shares the Top Chrome WebUI renderer process
// (see `TopChromeWebUIConfig::ProcessModel`). Returns false for WebUIs that
// are locked to their own site URL, which includes all `chrome-untrusted://`
// WebUIs.
bool IsTopChromeWebUIURL(const GURL& url);

// Returns true if `url` is the URL of an untrusted Top Chrome WebUI on desktop
// browsers. These URLs always have the format
// "chrome-untrusted://*.top-chrome".
bool IsTopChromeUntrustedWebUIURL(const GURL& url);

// Returns true if the Top Chrome WebUI at `url` should not become the key view.
bool ShouldRefuseBecomingKeyViewForTopChromeWebUI(const GURL& url);

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_URL_UTILS_H_
