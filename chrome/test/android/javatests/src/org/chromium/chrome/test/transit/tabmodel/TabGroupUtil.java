// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tabmodel;

/** Utilities for testing tab groups. */
public class TabGroupUtil {
    /**
     * Returns a String pluralizing a number of tabs saying "0 tabs", "1 tab", "2 tabs", etc.
     *
     * <p>English-specific, for testing.
     */
    public static String getNumberOfTabsString(int numberOfTabs) {
        return numberOfTabs != 1 ? String.format("%d tabs", numberOfTabs) : "1 tab";
    }
}
