// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTENT_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_CONTENT_TEST_UTILS_H_


namespace content {
class WebContents;
}

// Tests that text input in |contents| can be updated via key events. Note this
// function is destructive and replaces the page in |contents| with a input.
void TestTextInputViaKeyEvent(content::WebContents* contents);

#endif  // CHROME_BROWSER_UI_VIEWS_CONTENT_TEST_UTILS_H_
