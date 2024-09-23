// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.messages.snackbar;

import androidx.annotation.ColorInt;

public interface SnackbarStateProvider {
    /** An observer to be notified of changes to the overlay panel. */
    interface Observer {
        /**
         * Called when snackbar state changes.
         *
         * @param isShowing True if a snackbar is currently shown, false otherwise.
         * @param color The color of the current {@link Snackbar}.
         */
        default void onSnackbarStateChanged(boolean isShowing, @ColorInt Integer color) {}
    }

    /**
     * Add an observer to be notified of changes to the overlay panel.
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

    /** Return whether the snackbars extend across the full width of their container. */
    boolean isFullWidth();
}
