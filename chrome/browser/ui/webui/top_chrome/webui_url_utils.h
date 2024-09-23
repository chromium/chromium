// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_URL_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_URL_UTILS_H_

class GURL;

// Returns true if `url` is the URL of a trusted Top Chrome WebUI on desktop
// browsers. These URLs always have the format "chrome://*.top-chrome".
bool IsTopChromeWebUIURL(const GURL& url);

// Returns true if `url` is the URL of an untrusted Top Chrome WebUI on desktop
// browsers. These URLs always have the format
// "chrome-untrusted://*.top-chrome".
bool IsTopChromeUntrustedWebUIURL(const GURL& url);

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_URL_UTILS_H_
