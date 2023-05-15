// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.xsurface;

/**
 * Implemented internally.
 *
 * Interface providing helper methods to layout list items in an external surface-controlled
 * RecyclerView.
 */
public interface ListLayoutHelper {
    /**
     * Helper method to retrieve first visible item position from
     * @{@link androidx.recyclerview.widget.RecyclerView} based on layout type.
     * @return position of first visible item in list.
     */
    default int findFirstVisibleItemPosition() {
        return 0;
    }

    /**
     * Helper method to retrieve last visible item position from
     * @{@link androidx.recyclerview.widget.RecyclerView} based on layout type.
     * @return position of last visible item in list.
     */
    default int findLastVisibleItemPosition() {
        return 0;
    }

    /**
     * Helper method to scroll @{@link androidx.recyclerview.widget.RecyclerView} to position with
     * defined offset based on layout type.
     * @param position position to scroll to.
     * @param offset offset to scroll to.
     */
    default void scrollToPositionWithOffset(int position, int offset) {}

    /**
     * Sets column count for @{@link android.support.v7.widget.RecyclerView}
     * @param columnCount number of columns.
     * @return true if successful. false otherwise.
     */
    default boolean setColumnCount(int columnCount) {
        return false;
    }
}
