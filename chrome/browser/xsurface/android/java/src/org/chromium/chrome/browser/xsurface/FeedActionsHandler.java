// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.view.View;

import androidx.annotation.Nullable;

import java.util.Map;

/**
 * Interface to provide chromium calling points for a feed.
 */
public interface FeedActionsHandler {
    String KEY = "FeedActions";

    /**
     * Requests additional content to be loaded. Once the load is completed, onStreamUpdated will be
     * called.
     */
    default void loadMore() {}

    /**
     * Sends data back to the server when content is clicked.
     */
    default void processThereAndBackAgainData(byte[] data) {}

    /**
     * Sends data back to the server when content is clicked and provides the corresponding view
     * through |actionSourceView| which can be null.
     */
    @Deprecated
    default void processThereAndBackAgainData(byte[] data, @Nullable View actionSourceView) {}

    /**
     * Stores a view FeedAction for eventual upload. 'data' is a serialized FeedAction protobuf
     * message.
     */
    default void processViewAction(byte[] data) {}

    /**
     * Triggers Chrome to send user feedback for this card.
     */
    default void sendFeedback(Map<String, String> productSpecificDataMap) {}

    /**
     * Requests to dismiss a card. A change ID will be returned and it can be used to commit or
     * discard the change.
     * @param data A serialized feedpacking.DismissData message.
     */
    default int requestDismissal(byte[] data) {
        return 0;
    }

    /**
     * Commits a previous requested dismissal denoted by change ID.
     */
    default void commitDismissal(int changeId) {}

    /**
     * Discards a previous requested dismissal denoted by change ID.
     */
    default void discardDismissal(int changeId) {}

    /**
     * Interface for handling snackbar exit conditions.
     */
    public interface SnackbarController {
        /**
         * Called when the snackbar's action button is tapped.
         */
        default void onAction() {}
        /**
         * Called when the snackbar is dismissed without the button being tapped (usually when it
         * times out).
         */
        default void onDismissNoAction() {}
    }

    /**
     * Snackbar dismissal timeout.
     */
    public enum SnackbarDuration {
        /**
         * SHORT should be used with simple one-line snackbars.
         */
        SHORT,
        /**
         * LONG should be used with multi-line snackbars that take longer to read.
         */
        LONG
    }

    /**
     * Show a snackbar.
     * @param text Text to display.
     * @param actionLabel Text for the button (e.g. "Undo").
     * @param duration Whether to remove the snackbar after a short or long delay.
     * @param controller Handlers for snackbar actions.
     */
    default void showSnackbar(String text, String actionLabel, SnackbarDuration duration,
            SnackbarController controller) {}

    /**
     * Share a URL. This will bring up the sharing sheet.
     * @param url The url of the page to be shared.
     * @param title The title of the page to be shared.
     */
    default void share(String url, String title) {}
}
