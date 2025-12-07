// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ANDROID_ANDROID_UI_TEST_UTILS_H_
#define CHROME_TEST_BASE_ANDROID_ANDROID_UI_TEST_UTILS_H_

#include "url/gurl.h"

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

}  // namespace android_ui_test_utils

#endif  // CHROME_TEST_BASE_ANDROID_ANDROID_UI_TEST_UTILS_H_
