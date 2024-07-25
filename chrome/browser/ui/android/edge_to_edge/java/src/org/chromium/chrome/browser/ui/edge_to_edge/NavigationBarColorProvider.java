// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import androidx.annotation.ColorInt;

/** An interface for broadcasting changes to the navigation bar color. */
public interface NavigationBarColorProvider {
    /** An observer to be notified of changes to the navigation bar color. */
    interface Observer {
        /**
         * Called when the navigation bar color changes. Note that this color does not account for
         * edge-to-edge mode - when drawing edge-to-edge, the actual (OS) navigation bar will be set
         * to transparent, while this color will continue reflect the non-transparent "original"
         * navigation bar color.
         *
         * @param color The color of the navigation bar.
         */
        void onNavigationBarColorChanged(@ColorInt int color);

        /**
         * Called when the navigation bar has an update to the color of its divider.
         *
         * @param dividerColor The color of the navigation bar divider.
         */
        void onNavigationBarDividerChanged(@ColorInt int dividerColor);
    }

    /**
     * @return The current color for the navigation bar. Note that this color does not account for
     *     edge-to-edge mode - when drawing edge-to-edge, the actual (OS) navigation bar will be set
     *     to transparent, while this color will continue reflect the non-transparent "original"
     *     navigation bar color.
     */
    int getNavigationBarColor();

    /**
     * Add an observer to be notified of changes to the navigation bar color.
     *
     * @param observer The observer to add.
     */
    void addObserver(Observer observer);

    /**
     * Remove a previously added observer.
     *
     * @param observer The observer to remove.
     */
    void removeObserver(Observer observer);
}
