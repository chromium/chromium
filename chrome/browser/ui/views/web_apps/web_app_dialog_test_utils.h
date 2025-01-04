// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DIALOG_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DIALOG_TEST_UTILS_H_
#include "base/types/expected.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

// Open a popup window with the given URL and return its WebContents.
base::expected<content::WebContents*, std::string> OpenPopupOfSize(
    content::WebContents* contents,
    const GURL& url,
    int width = 200,
    int height = 100);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_DIALOG_TEST_UTILS_H_
