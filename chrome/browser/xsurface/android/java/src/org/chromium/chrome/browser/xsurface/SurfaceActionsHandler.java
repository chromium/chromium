// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
/**
 * Interface to provide chromium calling points for an external surface.
 */
public interface SurfaceActionsHandler {
    String KEY = "GeneralActions";

    @IntDef({OpenMode.UNKNOWN, OpenMode.SAME_TAB, OpenMode.NEW_TAB, OpenMode.INCOGNITO_TAB,
            OpenMode.DOWNLOAD_LINK, OpenMode.READ_LATER, OpenMode.THANK_CREATOR,
            OpenMode.NEW_TAB_IN_GROUP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OpenMode {
        int UNKNOWN = 0;
        // The URL is opened in the same tab (default).
        int SAME_TAB = 1;
        // The URL is opened in a new tab.
        int NEW_TAB = 2;
        // The URL is opened in an incognito tab.
        int INCOGNITO_TAB = 3;
        // The URL is downloaded.
        int DOWNLOAD_LINK = 4;
        // The URL is added for later reading.
        int READ_LATER = 5;
        // Deprecated. The URL to thank the current creator is opened in a Chrome Custom Tab (CCT).
        int THANK_CREATOR = 6;
        // The URL is opened in a new tab that is organized as group.
        int NEW_TAB_IN_GROUP = 7;
    }

    /**
     * Options for entry points to the single web feed.
     */
    @IntDef({OpenWebFeedEntryPoint.ATTRIBUTION, OpenWebFeedEntryPoint.RECOMMENDATION,
            OpenWebFeedEntryPoint.GROUP_HEADER, OpenWebFeedEntryPoint.OTHER,
            OpenWebFeedEntryPoint.MAX_VALUE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OpenWebFeedEntryPoint {
        /**
         * Feed Atteribution
         */
        int ATTRIBUTION = 0;
        /**
         * Feed Recomentation
         */
        int RECOMMENDATION = 1;
        /**
         * Group Header Attribution
         */
        int GROUP_HEADER = 2;
        /**
         * Other
         */
        int OTHER = 3;
        int MAX_VALUE = OTHER;
    }

    /**
     * Options when opening URLs with openUrl().
     */
    interface OpenUrlOptions {
        /**
         * The WebFeed associated with this navigation, for use with shouldShowWebFeedAccelerator(),
         * this is the WebFeed that should be followed if the user taps the accelerator.
         */
        default String webFeedName() {
            return "";
        }
        /** Whether to show the Web Feed accelerator on the page after navigation. */
        default boolean shouldShowWebFeedAccelerator() {
            return false;
        }
        /** Returns the title. Currently used only for READ_LATER. */
        default String getTitle() {
            return "";
        }
        /** The View from which the user tap originated. May be null.*/
        @Nullable
        default View actionSourceView() {
            return null;
        }
    }

    /**
     * Navigates the current tab to a particular URL.
     * @param openMode The OpenMode to use.
     * @param url The url for which to navigate.
     * @param options Additional options for opening the URL.
     */
    default void openUrl(@OpenMode int openMode, String url, OpenUrlOptions options) {}

    /**
     * Navigates the current tab to a particular URL.
     * @param url The url for which to navigate.
     * @param actionSourceView The View from which the user tap originated. May be null.
     */
    @Deprecated
    default void navigateTab(String url, View actionSourceView) {}

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
     * Opens a specific WebFeed by name.
     * @param webFeedName the relevant web feed name.
     */
    @Deprecated
    default void openWebFeed(String webFeedName) {
        openWebFeed(webFeedName, OpenWebFeedEntryPoint.OTHER);
    }

    /**
     * Opens a specific WebFeed by name with a specific entrypoint.
     * @param webFeedName the relevant web feed name.
     * @param entryPoint the entry point used to launch the feed.
     */
    default void openWebFeed(String webFeedName, @OpenWebFeedEntryPoint int entryPoint) {}

    /** Requests that a sync consent prompt be shown. */
    default void showSyncConsentPrompt() {}

    /** Requests that a sign-in interstitial bottom sheet be shown. */
    default void showSignInInterstitial() {}
}
