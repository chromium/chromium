// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_TEST_UTILS_H_
#define CHROME_TEST_BASE_CHROME_TEST_UTILS_H_

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

// This namespace contains test utilities that function for both Android and
// desktop browser tests.
namespace chrome_test_utils {

// Returns the active WebContents. On desktop this is in the first browser
// window created by tests, more specific behaviour requires other means.
content::WebContents* GetActiveWebContents(PlatformBrowserTest* browser_test);

}  // namespace chrome_test_utils

#endif  // CHROME_TEST_BASE_CHROME_TEST_UTILS_H_
