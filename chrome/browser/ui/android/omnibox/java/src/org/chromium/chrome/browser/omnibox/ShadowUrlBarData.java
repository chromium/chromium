// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/**
 * Shadow of the UrlBarData, that permits stubbing/mocking static methods for testing.
 */
@Implements(UrlBarData.class)
public class ShadowUrlBarData {
    public static boolean sShouldShowNextUrl = true;

    @Implementation
    public static boolean shouldShowUrl(String url, boolean isIncognito) {
        boolean res = sShouldShowNextUrl;
        sShouldShowNextUrl = true;
        return res;
    }
}
