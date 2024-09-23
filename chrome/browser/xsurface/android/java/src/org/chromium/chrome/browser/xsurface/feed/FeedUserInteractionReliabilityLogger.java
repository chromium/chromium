// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.feed;

import android.view.View;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// TODO(b/269234249): Don't use in Chromium yet, it's not implemented.
/**
 * Implemented internally.
 *
 * <p>Interface for capturing the reliability for every change in the collection of items presented
 * to the user during the interaction. One instance exists per feed surface and lasts for the
 * surface's lifetime.
 */
public interface FeedUserInteractionReliabilityLogger {
    /** Called when the stream has been opened. This should be called before reporting any event. */
    default void onStreamOpened(@StreamType int streamType) {}

    /** Describes how the stream is closed. */
    @IntDef({
        ClosedReason.OPEN_CARD,
        ClosedReason.SUSPEND_APP,
        ClosedReason.LEAVE_FEED,
        ClosedReason.SWITCH_STREAM
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ClosedReason {
        /** The user taps on a card. */
        int OPEN_CARD = 0;

        /** The user leaves the app. */
        int SUSPEND_APP = 1;

        /** The user leaves the feed but still stays in the app, like switching to other tab. */
        int LEAVE_FEED = 2;

        /** The user switches to other stream. */
        int SWITCH_STREAM = 3;
    }

    /** Called when the stream has been closed. */
    default void onStreamClosed(@ClosedReason int reason) {}

    /**
     * Called when the view has entered the visible part of the screen for the first time. If the
     * view is then off screen and become visible again, it will not be reported again.
     */
    default void onViewFirstVisible(View view) {}

    /**
     * Called when the view has been rendered for the first time. Note that this doesn't include the
     * child images. If the view is then off screen and become visible again, it will not be
     * reported again.
     */
    default void onViewFirstRendered(View view) {}

    /** Called when the pagination process has started. */
    default void onPaginationStarted() {}

    /**
     * Called when the waiting indicator is shown. This will happen after the pagination process
     * has started.
     */
    default void onPaginationIndicatorShown() {}

    /** Called when the user scrolled away from the waiting indicator for the pagination. */
    default void onPaginationUserScrolledAwayFromIndicator() {}

    /** Called when the action upload request has started. */
    default void onPaginationActionUploadRequestStarted() {}

    /** Called when the pagination query request has been sent. */
    default void onPaginationRequestSent() {}

    /**
     * Called when the pagination query response has been received.
     * @param serverRecvTimestamp Server-reported time (nanoseconds) at which the request arrived.
     * @param serverSendTimestamp Server-reported time (nanoseconds) at which the response was sent.
     */
    default void onPaginationResponseReceived(long serverRecvTimestamp, long serverSendTimestamp) {}

    /**
     * Called when the pagination query request has finished.
     * @param canonicalStatus Network request status code.
     */
    default void onPaginationRequestFinished(int canonicalStatus) {}

    /** Describes the end state of the pagination process. */
    @IntDef({
        PaginationResult.SUCCESS_WITH_MORE_FEED,
        PaginationResult.SUCCESS_WITH_NO_FEED,
        PaginationResult.FAILURE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PaginationResult {
        /** More feed content is retrieved. */
        int SUCCESS_WITH_MORE_FEED = 0;

        /** No feed content is retrieved. This means that the end of the feed is reached. */
        int SUCCESS_WITH_NO_FEED = 1;

        /** The pagination request has failed. */
        int FAILURE = 2;
    }

    /** Called when the pagination process has ended. */
    default void onPaginationEnded(@PaginationResult int result) {}

    /** Include experiment IDs sent from the server in the reliability log. */
    default void reportExperiments(int[] experimentIds) {}
}
