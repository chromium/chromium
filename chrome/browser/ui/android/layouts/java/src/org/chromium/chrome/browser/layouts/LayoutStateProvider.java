// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

/**
 * Exposes the current {@link Layout} state as well as a way to listen to {@link Layout} state
 * changes.
 */
public interface LayoutStateProvider {
    /** An observer that is notified when the {@link Layout} state changes. */
    interface LayoutStateObserver {
        /**
         * Called when Layout starts showing.
         *
         * @param layoutType LayoutType of the started showing Layout.
         */
        default void onStartedShowing(@LayoutType int layoutType) {}

        /**
         * Called when Layout finishes showing.
         * @param layoutType LayoutType of the finished showing Layout.
         */
        default void onFinishedShowing(@LayoutType int layoutType) {}

        /**
         * Called when Layout starts hiding.
         * @param layoutType LayoutType of the started hiding Layout.
         */
        default void onStartedHiding(@LayoutType int layoutType) {}

        /**
         * Called when Layout finishes hiding.
         * @param layoutType LayoutType of the finished hiding Layout.
         */
        default void onFinishedHiding(@LayoutType int layoutType) {}
    }

    /**
     * Determines whether the layout for a specific type is visible.
     * @return {@code true} if the {@link Layout} is visible, {@code false} otherwise.
     * @param layoutType The {@link LayoutType} of the {@link Layout} that is visible.
     */
    boolean isLayoutVisible(@LayoutType int layoutType);

    /**
     * Determines whether a layout has started and is in the process of hiding.
     * @return {@code true} if the {@link Layout} is starting to hide, {@code false} otherwise.
     * @param layoutType The {@link LayoutType} of the {@link Layout} that is starting to hide.
     */
    boolean isLayoutStartingToHide(@LayoutType int layoutType);

    /**
     * Determines whether a layout has started and is in the process of showing.
     * @return {@code true} if the {@link Layout} is starting to show, {@code false} otherwise.
     * @param layoutType The {@link LayoutType} of the {@link Layout} that is starting to show.
     */
    boolean isLayoutStartingToShow(@LayoutType int layoutType);

    /**
     * Gets the type of the layout that is currently active.
     * @return The {@link LayoutType} of the active layout.
     */
    @LayoutType
    int getActiveLayoutType();

    /**
     * @param listener Registers {@code listener} for all layout status changes.
     */
    void addObserver(LayoutStateObserver listener);

    /**
     * @param listener Unregisters {@code listener} for all layout status changes.
     */
    void removeObserver(LayoutStateObserver listener);

    /** Returns the ID of the next layout to show or {@code LayoutType.NONE} if one isn't set. */
    @LayoutType
    int getNextLayoutType();
}
