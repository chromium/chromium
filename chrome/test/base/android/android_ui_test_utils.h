// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ANDROID_ANDROID_UI_TEST_UTILS_H_
#define CHROME_TEST_BASE_ANDROID_ANDROID_UI_TEST_UTILS_H_

#include "url/gurl.h"

class BrowserWindowInterface;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

// A collections of functions designed for use with AndroidBrowserTest.
namespace android_ui_test_utils {

// Opens |url| in a new tab.
void OpenUrlInNewTab(content::BrowserContext* context,
                     content::WebContents* parent,
                     const GURL& url);

// Waits until `browser` shows the browsing layout (a tab's contents) and no
// layout transition is in progress. Returns at once if that is already so.
//
// Use it after anything that starts a layout transition, e.g. opening a tab in
// the foreground. On phones that runs the new tab animation, which selects the
// new tab again when it ends, so a tab switch made before then is undone.
void WaitForBrowsingLayout(BrowserWindowInterface* browser);

}  // namespace android_ui_test_utils

#endif  // CHROME_TEST_BASE_ANDROID_ANDROID_UI_TEST_UTILS_H_
