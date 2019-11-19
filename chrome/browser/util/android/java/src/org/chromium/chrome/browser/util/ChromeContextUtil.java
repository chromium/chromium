// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;

final class ChromeContextUtil {
    private ChromeContextUtil() {}

    @CalledByNative
    private static int getSmallestDIPWidth() {
        return ContextUtils.getApplicationContext()
                .getResources()
                .getConfiguration()
                .smallestScreenWidthDp;
    }
}
