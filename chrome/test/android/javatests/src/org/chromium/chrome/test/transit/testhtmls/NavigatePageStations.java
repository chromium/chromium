// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.testhtmls;

import org.chromium.chrome.test.transit.PageStation.Builder;
import org.chromium.chrome.test.transit.WebPageStation;

/** PageStations for /chrome/test/data/android/navigate/*.html */
public abstract class NavigatePageStations {
    /** Create a PageStation representing one.html. */
    public static Builder<WebPageStation> newNavigateOnePageBuilder() {
        return WebPageStation.newWebPageStationBuilder()
                .withPath("/chrome/test/data/android/navigate/one.html")
                .withTitle("One");
    }

    /** Create a PageStation representing two.html. */
    public static Builder<WebPageStation> newNavigateTwoPageBuilder() {
        return WebPageStation.newWebPageStationBuilder()
                .withPath("/chrome/test/data/android/navigate/two.html")
                .withTitle("Two");
    }

    /** Create a PageStation representing three.html. */
    public static Builder<WebPageStation> newNavigateThreePageBuilder() {
        return WebPageStation.newWebPageStationBuilder()
                .withPath("/chrome/test/data/android/navigate/three.html")
                .withTitle("Three");
    }

    /** Create a PageStation representing simple.html. */
    public static Builder<WebPageStation> newNavigateSimplePageBuilder() {
        return WebPageStation.newWebPageStationBuilder()
                .withPath("/chrome/test/data/android/navigate/simple.html")
                .withTitle("Simple");
    }
}
