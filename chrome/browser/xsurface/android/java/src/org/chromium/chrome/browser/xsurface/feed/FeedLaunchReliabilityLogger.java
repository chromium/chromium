// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.feed;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Implemented internally.
 *
 * <p>Interface for logging latency and availability signals for feed launches. All timestamps are
 * in terms of nanoseconds since system boot. One instance exists per feed surface and lasts for the
 * surface's lifetime.
 */
public interface FeedLaunchReliabilityLogger {
    @IntDef({SurfaceType.UNSPECIFIED, SurfaceType.NEW_TAB_PAGE, SurfaceType.START_SURFACE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SurfaceType {
        int UNSPECIFIED = 0;
        int NEW_TAB_PAGE = 1;
        @Deprecated int START_SURFACE = 2;
    }

    @IntDef({
        StreamType.UNSPECIFIED,
        StreamType.FOR_YOU,
        StreamType.WEB_FEED,
        StreamType.SINGLE_WEB_FEED,
        StreamType.SUPERVISED_USER_FEED
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface StreamType {
        int UNSPECIFIED = 0;
        int FOR_YOU = 1;
        int WEB_FEED = 2;
        int SINGLE_WEB_FEED = 3;
        int SUPERVISED_USER_FEED = 4;
    }

    /**
     * Set details about the stream being launched and send any pending events.
     *
     * @param streamType Feed type (e.g. "for you", "following" or "supervised user").
     * @param streamId Identifier for the stream used to disambiguate events from concurrent
     *     streams.
     */
    default void sendPendingEvents(
            @org.chromium.chrome.browser.xsurface.feed.StreamType int streamType, int streamId) {}

    /** Clear any pending events and end the flow without logging any events. */
    default void cancelPendingEvents() {}

    /**
     * Returns true if logUiStarting(), logFeedReloading(), or logFeedLaunchOtherStart() have been
     * called since the last call to logLaunchFinished().
     * @return True if the launch flow has started but not finished.
     */
    default boolean isLaunchInProgress() {
        return false;
    }

    /**
     * Log when the feed is launched because its UI surface was created.
     * @param surfaceType Feed surface type (e.g. new tab page or Start Surface).
     */
    default void logUiStarting(@SurfaceType int surfaceType) {}

    /** Log when a feed refresh is requested manually. */
    default void logManualRefresh() {}

    /**
     * Log when the feed is launched because its surface was shown and cards needed to be
     * re-rendered.
     */
    default void logFeedReloading() {}

    /**
     * Log when the feed is launched in any case not already handled by logUiStarting() or
     * logFeedReloaded().
     */
    default void logFeedLaunchOtherStart() {}

    /**
     * Log when the user switches to another feed tab.
     * @param toStreamType New feed type.
     */
    default void logSwitchedFeeds(
            @org.chromium.chrome.browser.xsurface.feed.StreamType int toStreamType) {}

    /** Log when cached feed content is about to be read. */
    default void logCacheReadStart() {}

    /**
     * Log after finishing attempting to read cached feed content.
     * @param result DiscoverCardReadCacheResult.
     */
    default void logCacheReadEnd(int result) {}

    /** Log when the loading spinner is shown. */
    default void logLoadingIndicatorShown() {}

    /** Log when rendering of above-the-fold feed content begins. */
    default void logAtfRenderStart() {}

    /**
     * Log when rendering of above-the-fold feed content finishes.
     * @param result DiscoverAboveTheFoldRenderResult.
     */
    default void logAtfRenderEnd(int result) {}

    /**
     * Get the network request logger for a request by its ID.
     * @param requestId A unique ID for the request.
     * @return A logger for the request: an existing logger if one matches `requestId`, or a new one
     *         otherwise.
     */
    default FeedNetworkRequestReliabilityLogger getNetworkRequestReliabilityLogger2(int requestId) {
        return new FeedNetworkRequestReliabilityLogger() {};
    }

    /**
     * Log to mark the end of the feed launch. Logs a "launched finished" event with the result
     * (or instead with the pending "launch finished" result if there was a call to
     * pendingFinished()).
     * @param result DiscoverLaunchResult.
     */
    default void logLaunchFinished(int result) {}

    /**
     * Log to mark the end of the feed launch.
     * @param result DiscoverLaunchResult.
     * @param onlyIfLaunchInProgress Pass true if this event should only be logged if there is a
     *         feed launch in progress.
     */
    default void logLaunchFinished(int result, boolean onlyIfLaunchInProgress) {}

    /**
     * Keep a tentative status for "launch finished" if the user left the feed but might return
     * before it finishes loading.
     * If the next call is to logLaunchFinished(), logLaunchFinished() will log the pending
     * "launch finished" status and clear them. If the next call is to cancelPendingFinished(), the
     * pending "launch finished" is cleared. If there is already a pending "launch finished",
     * calling pendingFinished() again has no effect.
     * @param result DiscoverLaunchResult.
     */
    default void pendingFinished(int result) {}

    /** Drop anything kept with pendingFinished(). */
    default void cancelPendingFinished() {}

    /** Include experiment IDs sent from the server in the reliability log. */
    default void reportExperiments(int[] experimentIds) {}

    /**
     * Log when the feed is launched because its UI surface was created.
     *
     * @param surfaceType Feed surface type (e.g. new tab page or Start Surface).
     * @param timestamp Time at which the surface began to be created.
     */
    @Deprecated
    default void logUiStarting(@SurfaceType int surfaceType, long timestamp) {}

    /**
     * Log when a feed refresh is requested manually.
     * @param timestamp Time at which the surface was shown.
     */
    @Deprecated
    default void logManualRefresh(long timestamp) {}

    /**
     * Log when the feed is launched because its surface was shown and cards needed to be
     * re-rendered.
     * @param timestamp Time at which the surface was shown.
     */
    @Deprecated
    default void logFeedReloading(long timestamp) {}

    /**
     * Log when the feed is launched in any case not already handled by logUiStarting() or
     * logFeedReloaded().
     * @param timestamp Time at which the feed stream was bound.
     */
    @Deprecated
    default void logFeedLaunchOtherStart(long timestamp) {}

    /**
     * Log when the user switches to another feed tab.
     * @param toStreamType New feed type.
     * @param timestamp Event time.
     */
    @Deprecated
    default void logSwitchedFeeds(
            @org.chromium.chrome.browser.xsurface.feed.StreamType int toStreamType,
            long timestamp) {}

    /**
     * Log when cached feed content is about to be read.
     * @param timestamp Event time.
     */
    @Deprecated
    default void logCacheReadStart(long timestamp) {}

    /**
     * Log after finishing attempting to read cached feed content.
     * @param timestamp Event time.
     * @param result DiscoverCardReadCacheResult.
     */
    @Deprecated
    default void logCacheReadEnd(long timestamp, int result) {}

    /**
     * Log when the loading spinner is shown.
     * @param timestamp Time at which the spinner was shown.
     */
    @Deprecated
    default void logLoadingIndicatorShown(long timestamp) {}

    /**
     * Log when rendering of above-the-fold feed content begins.
     * @param timestamp Event time.
     */
    @Deprecated
    default void logAtfRenderStart(long timestamp) {}

    /**
     * Log when rendering of above-the-fold feed content finishes.
     * @param timestamp Event time.
     * @param result DiscoverAboveTheFoldRenderResult.
     */
    @Deprecated
    default void logAtfRenderEnd(long timestamp, int result) {}

    /**
     * Log to mark the end of the feed launch. Logs a "launched finished" event with the timestamp
     * and result (or instead with the pending "launch finished" timestamp and result if there was a
     * call to pendingFinished()).
     * @param timestamp Event time, possibly the same as one of the other events.
     * @param result DiscoverLaunchResult.
     */
    @Deprecated
    default void logLaunchFinished(long timestamp, int result) {}

    /**
     * Log to mark the end of the feed launch.
     * @param timestamp Event time, possibly the same as one of the other events.
     * @param result DiscoverLaunchResult.
     * @param onlyIfLaunchInProgress Pass true if this event should only be logged if there is a
     *         feed launch in progress.
     */
    @Deprecated
    default void logLaunchFinished(long timestamp, int result, boolean onlyIfLaunchInProgress) {}

    /**
     * Keep a tentative timestamp and status for "launch finished" if the user left the feed but
     * might return before it finishes loading.
     * If the next call is to logLaunchFinished(), logLaunchFinished() will log the pending
     * "launch finished" timestamp and status and clear them. If the next call is to
     * cancelPendingFinished(), the pending "launch finished" is cleared. If there is already a
     * pending "launch finished", calling pendingFinished() again has no effect.
     * @param timestamp Event time in nanoseconds.
     * @param result DiscoverLaunchResult.
     */
    @Deprecated
    default void pendingFinished(long timestamp, int result) {}
}
