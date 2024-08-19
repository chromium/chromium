// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.testhtmls;

import org.chromium.chrome.test.transit.page.PageStation.Builder;
import org.chromium.chrome.test.transit.page.WebPageStation;

/** PageStations for /chrome/test/data/android/navigate/*.html */
public abstract class NavigatePageStations {
    public static final String PATH_ONE = "/chrome/test/data/android/navigate/one.html";
    public static final String PATH_TWO = "/chrome/test/data/android/navigate/two.html";
    public static final String PATH_THREE = "/chrome/test/data/android/navigate/three.html";
    public static final String PATH_SIMPLE = "/chrome/test/data/android/navigate/simple.html";

    /** Create a PageStation representing one.html. */
    public static Builder<WebPageStation> newNavigateOnePageBuilder() {
        return WebPageStation.newBuilder()
                .withExpectedUrlSubstring(PATH_ONE)
                .withExpectedTitle("One");
    }

    /** Create a PageStation representing two.html. */
    public static Builder<WebPageStation> newNavigateTwoPageBuilder() {
        return WebPageStation.newBuilder()
                .withExpectedUrlSubstring(PATH_TWO)
                .withExpectedTitle("Two");
    }

    /** Create a PageStation representing three.html. */
    public static Builder<WebPageStation> newNavigateThreePageBuilder() {
        return WebPageStation.newBuilder()
                .withExpectedUrlSubstring(PATH_THREE)
                .withExpectedTitle("Three");
    }

    /** Create a PageStation representing simple.html. */
    public static Builder<WebPageStation> newNavigateSimplePageBuilder() {
        return WebPageStation.newBuilder()
                .withExpectedUrlSubstring(PATH_SIMPLE)
                .withExpectedTitle("Simple");
    }
}
