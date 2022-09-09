// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.view.View;

import androidx.annotation.Nullable;

import java.util.List;
/**
 * Interface to provide chromium calling points for an external surface.
 */
public interface SurfaceActionsHandler {
    String KEY = "GeneralActions";

    /**
     * Navigates the current tab to a particular URL.
     * @param url The url for which to navigate.
     * @param actionSourceView The View from which the user tap originated. May be null.
     */
    default void navigateTab(String url, View actionSourceView) {}

    /**
     * Navigates a new tab to a particular URL.
     * @param url The url for which to navigate.
     * @param actionSourceView The View from which the user tap originated. May be null.
     */
    default void navigateNewTab(String url, View actionSourceView) {}

    /**
     * Navigate a new incognito tab to a URL.
     */
    default void navigateIncognitoTab(String url) {}

    /**
     * Get an offline page for a URL.
     */
    default void downloadLink(String url) {}

    /** Add the url to the reading list and make it available offline. */
    default void addToReadingList(String title, String url) {}

    /** Opens Crow CCT for the URL. */
    default void navigateCrow(String url) {}

    /**
     * Open a bottom sheet with the view as contents.
     * @param view The bottom sheet contents view.
     * @param actionSourceView The View from which the user tap originated. May be null.
     */
    default void showBottomSheet(View view, View actionSourceView) {}

    /**
     * Dismiss the open bottom sheet (or do nothing if there isn't one).
     *
     */
    default void dismissBottomSheet() {}

    /**
     * Notifies the host app that url with broadTopicMids and entityMids was clicked.
     * @param url The URL that the user clicked on
     * @param entityMids Sorted list (most relevant to least) of entity MIDs that correspond to the
     *         clicked URL
     * @param contentCategoryMediaType MediaType expresses the primary media format of the content
     * @param cardCategory Expresses the category of the clicked card
     * TODO(tbansal): Remove the first method once the callers have been updated.
     */
    default void updateUserProfileOnLinkClick(String url, List<Long> entityMids) {}
    default void updateUserProfileOnLinkClick(
            String url, List<Long> entityMids, long contentCategoryMediaType, long cardCategory) {}

    /** A request to follow or unfollow a WebFeed. */
    interface WebFeedFollowUpdate {
        /**
         * Called after a WebFeedFollowUpdate completes, reporting whether or not it is successful.
         * For durable requests, this reports the status of the first attempt. Subsequent attempts
         * do not trigger this callback.
         */
        interface Callback {
            void requestComplete(boolean success);
        }

        /** The WebFeed name (ID) being operated on. */
        String webFeedName();

        /** Whether to follow, or unfollow the WebFeed. */
        default boolean isFollow() {
            return true;
        }

        /**
         * Whether the request will be automatically retried later if it fails (for example, due to
         * a network error).
         */
        default boolean isDurable() {
            return false;
        }

        /** The callback to be informed of completion, or null. */
        @Nullable
        default WebFeedFollowUpdate.Callback callback() {
            return null;
        }

        /** The WebFeedChangeReason for this change. */
        default int webFeedChangeReason() {
            return 0;
        }
    }

    /**
     * Attempts to follow or unfollow a WebFeed.
     */
    default void updateWebFeedFollowState(WebFeedFollowUpdate update) {}

    /**
     * Navigates a new tab in group to a particular URL.
     * @param url The url for which to navigate.
     * @param actionSourceView The View from which the user tap originated. May be null.
     */
    default void navigateNewTabInGroup(String url, View actionSourceView) {}
}
