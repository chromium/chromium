// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.feed;

import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.xsurface.LoggingParameters;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Map;

/**
 * Implemented in Chromium.
 *
 * Interface to provide chromium calling points for a feed.
 */
public interface FeedActionsHandler {
    String KEY = "FeedActions";

    /**
     * Requests additional content to be loaded. Once the load is completed, onStreamUpdated will be
     * called.
     */
    default void loadMore() {}

    /** Sends data back to the server when content is clicked. */
    default void processThereAndBackAgainData(byte[] data, LoggingParameters loggingParameters) {}

    /** Triggers Chrome to send user feedback for this card. */
    default void sendFeedback(Map<String, String> productSpecificDataMap) {}

    /**
     * Requests to dismiss a card. A change ID will be returned and it can be used to commit or
     * discard the change.
     * @param data A serialized feedpacking.DismissData message.
     */
    default int requestDismissal(byte[] data) {
        return 0;
    }

    /** Commits a previous requested dismissal denoted by change ID. */
    default void commitDismissal(int changeId) {}

    /** Discards a previous requested dismissal denoted by change ID. */
    default void discardDismissal(int changeId) {}

    /** Interface for handling snackbar exit conditions. */
    public interface SnackbarController {
        @Deprecated
        default void onAction() {}

        /**
         * Called when the snackbar's action button is tapped.
         *
         * @param actionFinished Should be called when work associated with this action has been
         *     completed.
         */
        default void onAction(Runnable actionFinished) {}

        @Deprecated
        default void onDismissNoAction() {}

        /**
         * Called when the snackbar is dismissed without the button being tapped (usually when it
         * times out).
         *
         * @param actionFinished Should be called when work associated with this action has been
         *     completed.
         */
        default void onDismissNoAction(Runnable actionFinished) {}
    }

    /** Snackbar dismissal timeout. */
    @IntDef({SnackbarDuration.SHORT, SnackbarDuration.LONG})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SnackbarDuration {
        /** SHORT should be used with simple one-line snackbars. */
        int SHORT = 0;

        /** LONG should be used with multi-line snackbars that take longer to read. */
        int LONG = 1;
    }

    /**
     * Show a snackbar.
     * @param text Text to display.
     * @param actionLabel Text for the button (e.g. "Undo").
     * @param duration Whether to remove the snackbar after a short or long delay.
     * @param controller Handlers for snackbar actions.
     */
    default void showSnackbar(
            String text,
            String actionLabel,
            @SnackbarDuration int duration,
            SnackbarController controller) {}

    /**
     * Share a URL. This will bring up the sharing sheet.
     * @param url The url of the page to be shared.
     * @param title The title of the page to be shared.
     */
    default void share(String url, String title) {}

    /** Opens the settings to manager autoplay. */
    default void openAutoplaySettings() {}

    /**
     * Watches a view to get notified when the first time it has the visible area percentage not
     * less than the given threshold. The watch is based on the visibility of full
     * ListContentManager item containing the view.
     * @param view The view to watch for.
     * @param viewedThreshold The threshold of the percentage of the visible area on screen.
     * @param runnable The runnable to get notified.
     */
    default void watchForViewFirstVisible(View view, float viewedThreshold, Runnable runnable) {}

    /**
     * Reports that the notice identified by the given key is created. It may not be visible yet.
     * @param key Key to identify the type of the notice. For each new key, please update
     *            "NoticeKey" token in histograms.xml and NoticeUmaName() in metrics_reporter.cc.
     */
    default void reportNoticeCreated(String key) {}

    /**
     * Reports that the notice identified by the given key is viewed, fully visible in the viewport.
     * @param key Key to identify the type of the notice. This interaction info can be used to
     * determine if it is necessary to show the notice to the user again.
     */
    default void reportNoticeViewed(String key) {}

    /**
     * Reports that the user has clicked/tapped the notice identified by the given key to perform
     * an open action. This interaction info can be used to determine if it is necessary to show
     * the notice to the user again.
     * @param key Key to identify the type of the notice.
     */
    default void reportNoticeOpenAction(String key) {}

    /**
     * Reports that the notice identified by the given key is dismissed by the user.
     * @param key Key to identify the type of the notice.
     */
    default void reportNoticeDismissed(String key) {}

    /**
     * Types of feeds that can be invalidated. These values must match the privately defined values
     * of InvalidateCacheData.FeedType.
     */
    @IntDef({
        FeedIdentifier.UNSPECIFIED,
        FeedIdentifier.MAIN_FEED,
        FeedIdentifier.FOLLOWING_FEED,
        FeedIdentifier.CHANNEL_FEED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FeedIdentifier {
        int UNSPECIFIED = 0;
        int MAIN_FEED = 1;
        int FOLLOWING_FEED = 2;
        int CHANNEL_FEED = 3;
    }

    /**
     * Requests that the cache a feed be invalidated so that its contents are re-fetched the next
     * time the feed is shown/loaded.
     * @param feedToInvalidate Identifies which feed should have its cache invalidated. The request
     *         will be dropped if set to FeedIdentifier.UNSPECIFIED.
     */
    default void invalidateContentCacheFor(@FeedIdentifier int feedToInvalidate) {}

    /**
     * Reports that the info card is being tracked for its full visibility.
     * @param type Type of the info card.
     */
    default void reportInfoCardTrackViewStarted(int type) {}

    /**
     * Reports that the info card is fully visible in the viewport.
     * @param type Type of the info card.
     * @param minimumViewIntervalSeconds The minimum interval in seconds from the last time the info
     * card is viewed in order for it to be considered viewed again.
     */
    default void reportInfoCardViewed(int type, int minimumViewIntervalSeconds) {}

    /**
     * Reports that the user tapps the info card.
     * @param type Type of the info card.
     */
    default void reportInfoCardClicked(int type) {}

    /**
     * Reports that the user dismisses the info card explicitly by tapping the close button.
     * @param type Type of the info card.
     */
    default void reportInfoCardDismissedExplicitly(int type) {}

    /**
     * Resets all the states of the info card.
     * @param type Type of the info card.
     */
    default void resetInfoCardStates(int type) {}

    /**
     * Reports that a piece of content was viewed.
     * @param docid Uniquely identifies the content.
     */
    default void contentViewed(long docid) {}

    /** Triggers a manual refresh of the feed. */
    default void triggerManualRefresh() {}
}
