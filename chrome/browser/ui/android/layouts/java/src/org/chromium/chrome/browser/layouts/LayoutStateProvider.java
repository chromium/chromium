// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

/**
 * Exposes the current {@link Layout} state as well as a way to listen to {@link Layout} state
 * changes.
 */
public interface LayoutStateProvider {
    /**
     * An observer that is notified when the {@link Layout} state changes.
     */
    interface LayoutStateObserver {
        // TODO(crbug.com/1108496): Reiterate to see whether the showToolbar param is needed.
        /**
         * Called when Layout starts showing.
         * @param layoutType LayoutType of the started showing Layout.
         * @param showToolbar Whether or not to show the normal toolbar when animating into the
         */
        default void onStartedShowing(@LayoutType int layoutType, boolean showToolbar) {}

        /**
         * Called when Layout finishes showing.
         * @param layoutType LayoutType of the finished showing Layout.
         */
        default void onFinishedShowing(@LayoutType int layoutType) {}

        // TODO(crbug.com/1108496): Reiterate to see whether the showToolbar and delayAnimation
        // param is needed.
        /**
         * Called when Layout starts hiding.
         * @param layoutType LayoutType of the started hiding Layout.
         * @param showToolbar    Whether or not to show the normal toolbar when animating out of
         *                       showing Layout.
         * @param delayAnimation Whether or not to delay any related animations until after Layout
         */
        default void onStartedHiding(
                @LayoutType int layoutType, boolean showToolbar, boolean delayAnimation) {}

        /**
         * Called when Layout finishes hiding.
         * @param layoutType LayoutType of the finished hiding Layout.
         */
        default void onFinishedHiding(@LayoutType int layoutType) {}

        /**
         * Called when a layout wants to hint that a new tab might be selected soon. This is not
         * called every time a tab is selected.
         * @param tabId The id of the tab that might be selected soon.
         */
        default void onTabSelectionHinted(int tabId) {}
    }

    /**
     * @return Whether or not the {@link Layout} is visible.
     * @param layoutType whether the {@link Layout} give {@link LayoutType} is visible.
     */
    boolean isLayoutVisible(@LayoutType int layoutType);

    /**
     * @return Whether or not the {@link Layout} is starting to hide.
     * @param layoutType whether the {@link Layout} give {@link LayoutType} is starting to hide.
     */
    boolean isLayoutStartingToHide(@LayoutType int layoutType);

    /**
     * Get the type of the layout that is currently active.
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
}