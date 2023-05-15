// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

/**
 * Implemented in Chromium.
 *
 * Interface to observe a list.
 */
public interface ListContentManagerObserver {
    /** Called when range from startIndex to startIndex+count has been inserted. */
    default void onItemRangeInserted(int startIndex, int count) {}

    /** Called when range from startIndex to startIndex+count has been removed. */
    default void onItemRangeRemoved(int startIndex, int count) {}

    /** Called when range from startIndex to startIndex+count has been changed/updated. */
    default void onItemRangeChanged(int startIndex, int count) {}

    /** Called when item at curIndex has been moved to newIndex. */
    default void onItemMoved(int curIndex, int newIndex) {}
}
