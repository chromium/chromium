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

    /**
     * Returns a String pluralizing a number of items saying "0 items", "1 item", "2 items", etc.
     *
     * <p>English-specific, for testing.
     */
    public static String getNumberOfItemsString(int numberOfItems) {
        return numberOfItems != 1 ? String.format("%d items", numberOfItems) : "1 item";
    }

    /**
     * Returns the message shown on the snackbar when tabs get grouped, e.g. "3 tabs grouped".
     *
     * <p>English-specific, for testing.
     */
    public static String getUndoGroupTabsSnackbarMessageString(int numberOfTabs) {
        return getNumberOfTabsString(numberOfTabs) + " grouped";
    }

    /**
     * Returns the message shown on the snackbar when tab groups get closed (hidden). This assumes
     * tab group sync is enabled.
     *
     * <p>English-specific, for testing.
     */
    public static String getUndoCloseGroupSnackbarMessageString(String groupTitle) {
        return groupTitle + " tab group closed and saved";
    }
}
