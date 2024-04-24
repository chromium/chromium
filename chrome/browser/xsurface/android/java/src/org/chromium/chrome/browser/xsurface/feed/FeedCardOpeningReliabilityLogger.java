// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.feed;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Implemented internally.
 *
 * <p>Interface for capturing the reliability for user tapping a card. One instance exists per feed
 * surface and lasts for the surface's lifetime.
 */
public interface FeedCardOpeningReliabilityLogger {
    /** Describes the page loading error. */
    @IntDef({
        PageLoadError.INTERNET_DISCONNECTED,
        PageLoadError.CONNECTION_TIMED_OUT,
        PageLoadError.NAME_RESOLUTION_FAILED,
        PageLoadError.PAGE_LOAD_ERROR
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PageLoadError {
        /** The Internet connection has been lost. */
        int INTERNET_DISCONNECTED = 0;

        /** A connection attempt timed out. */
        int CONNECTION_TIMED_OUT = 1;

        /** An error occurred when trying to do a name resolution (DNS) */
        int NAME_RESOLUTION_FAILED = 2;

        /** Other error occurred. */
        int PAGE_LOAD_ERROR = 3;
    }

    /**
     * Called when the card is clicked.
     *
     * @param pageId The unique ID for the page being opened.
     * @param cardCategory The breakdown of cards into categories.
     */
    default void onCardClicked(int pageId, int cardCategory) {}

    /**
     * Called when the page starts loading.
     *
     * @param pageId The unique ID for the page being opened.
     */
    default void onPageLoadStarted(int pageId) {}

    /**
     * Called when the page finishes loading successfully.
     *
     * @param pageId The unique ID for the page being opened.
     */
    default void onPageLoadFinished(int pageId) {}

    /**
     * Called when the page fails to load.
     *
     * @param pageId The unique ID for the page being opened.
     * @param errorCode The error code that causes the page to fail loading.
     */
    default void onPageLoadFailed(int pageId, @PageLoadError int errorCode) {}

    /**
     * Called when the page finishes first paint after non-empty layout.
     *
     * @param pageId The unique ID for the page being opened.
     */
    default void onPageFirstContentfulPaint(int pageId) {}
}
