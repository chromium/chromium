// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.url.GURL;

/** Shadow of the UrlBarData, that permits stubbing/mocking static methods for testing. */
@Implements(UrlBarData.class)
public class ShadowUrlBarData {
    public static boolean sShouldShowNextUrl = true;

    @Implementation
    public static boolean shouldShowUrl(GURL gurl, boolean isIncognito) {
        boolean res = sShouldShowNextUrl;
        sShouldShowNextUrl = true;
        return res;
    }
}
