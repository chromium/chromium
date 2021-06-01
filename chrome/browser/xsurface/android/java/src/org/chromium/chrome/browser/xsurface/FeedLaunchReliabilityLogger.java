// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Interface for logging latency and availability signals for feed launches. All timestamps are in
 * terms of nanoseconds since system boot.
 */
public interface FeedLaunchReliabilityLogger {
    /** Type of surface the feed is being launched on. */
    @IntDef({SurfaceType.UNSPECIFIED, SurfaceType.NEW_TAB_PAGE, SurfaceType.START_SURFACE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SurfaceType {
        int UNSPECIFIED = 0;
        int NEW_TAB_PAGE = 1;
        int START_SURFACE = 2;
    }

    /** Type of stream being launched (the "For you" or "Following" feed). */
    @IntDef({StreamType.UNSPECIFIED, StreamType.FOR_YOU, StreamType.WEB_FEED})
    @Retention(RetentionPolicy.SOURCE)
    @interface StreamType {
        int UNSPECIFIED = 0;
        int FOR_YOU = 1;
        int WEB_FEED = 2;
    }

    /**
     * Sets details about the stream to be logged as metadata.
     * @param surfaceType Whether the feed is on the new tab page or Start Surface.
     * @param streamType Whether the feed is a "for you" feed or a "following" feed.
     * @param streamId Identifier for the stream used to disambiguate events from concurrent
     *         streams.
     */
    default void setMetadata(
            @SurfaceType int surfaceType, @StreamType int streamType, int streamId) {}

    /**
     * Log when the feed is launched because the NTP or Start Surface was created.
     * @param timestamp Time at which the surface began to be created.
     */
    default void logDiscoverUiStarting(long timestamp) {}

    /**
     * Log when the feed is launched because its surface was shown and cards needed to be
     * re-rendered.
     * @param timestamp Time at which the surface was shown.
     */
    default void logDiscoverFeedReloaded(long timestamp) {}

    /**
     * Log when the feed is launched in any case not already handled by logDiscoverUiStarting() or
     * logDiscoverFeedReloaded().
     * @param timestamp Time at which the feed stream was bound.
     */
    default void logDiscoverFeedLaunchOtherStart(long timestamp) {}

    /**
     * Log when cached feed content is about to be read.
     * @param timestamp Event time.
     */
    default void logDiscoverCacheReadStart(long timestamp) {}

    /**
     * Log after finishing attempting to read cached feed content.
     * @param timestamp Event time.
     * @param result DiscoverCardReadCacheResult.
     */
    default void logDiscoverCacheReadEnd(long timestamp, int result) {}

    /**
     * Log when the loading spinner is shown.
     * @param timestamp Time at which the spinner was shown.
     */
    default void logDiscoverLoadingIndicatorShown(long timestamp) {}

    /**
     * Log when rendering of above-the-fold feed content begins.
     * @param timestamp Event time.
     */
    default void logDiscoverAtfRenderStart(long timestamp) {}

    /**
     * Log when rendering of above-the-fold feed content finishes.
     * @param timestamp Event time.
     * @param result DiscoverAboveTheFoldRenderResult.
     */
    default void logDiscoverAtfRenderEnd(long timestamp, int result) {}

    /**
     * Log when making a feed query request.
     * @param timestamp Event time.
     * @param requestId Unique ID for this network request.
     * @return A logger for this network request.
     */
    @Nullable
    default FeedNetworkRequestReliabilityLogger logFeedQueryRequestStart(
            long timestamp, int requestId) {
        return null;
    }

    /**
     * Log just before making a feed actions upload request.
     * @param timestamp Event time.
     * @param requestId Unique ID for this network request.
     * @return A logger for this network request.
     */
    @Nullable
    default FeedNetworkRequestReliabilityLogger logActionsUploadRequestStart(
            long timestamp, int requestId) {
        return null;
    }

    /**
     * Log to mark the end of the feed launch.
     * @param timestamp Event time, possibly the same as one of the other events.
     * @param result DiscoverLaunchResult.
     */
    default void logDiscoverLaunchFinished(long timestamp, int result) {}
}