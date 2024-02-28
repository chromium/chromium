// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_URL_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_URL_UTILS_H_

class GURL;

// Returns true if `url` is the URL of a Top Chrome WebUI on
// desktop browsers. Such a URL ends with the ".top-chrome" TLD.
bool IsTopChromeWebUIURL(const GURL& url);

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_URL_UTILS_H_
