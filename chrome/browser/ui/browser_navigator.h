// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_
#define CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_

class GURL;

namespace content {
class BrowserContext;
}

struct NavigateParams;

// Navigates according to the configuration specified in |params|.
void Navigate(NavigateParams* params);

// Returns true if the url is allowed to open in incognito window.
bool IsURLAllowedInIncognito(const GURL& url,
                             content::BrowserContext* browser_context);

#endif  // CHROME_BROWSER_UI_BROWSER_NAVIGATOR_H_
