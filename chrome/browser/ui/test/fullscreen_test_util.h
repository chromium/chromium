// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_FULLSCREEN_TEST_UTIL_H_
#define CHROME_BROWSER_UI_TEST_FULLSCREEN_TEST_UTIL_H_

namespace content {

class WebContents;

// Waits for HTML fullscreen of `contents`; returns immediately if already so.
// https://fullscreen.spec.whatwg.org/
void WaitForHTMLFullscreen(WebContents* contents);

// Waits for HTML fullscreen of `contents` to exit; returns immediately if
// already so. https://fullscreen.spec.whatwg.org/
void WaitForHTMLFullscreenExit(WebContents* contents);

}  // namespace content

#endif  // CHROME_BROWSER_UI_TEST_FULLSCREEN_TEST_UTIL_H_
