// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.feed;

/**
 * Implemented internally.
 *
 * Interface for logging latency and availability signals for feed network requests. All timestamps
 * are in terms of nanoseconds since system boot.
 *
 * Obtain instances from FeedLaunchReliabilityLogger.getNetworkRequestReliabilityLogger().
 */
public interface FeedNetworkRequestReliabilityLogger {
    /**
     * Log before filling out and serializing a
     * feed query request. Starts the network
     * request flow.
     * @param timestamp Event time.
     */
    default void logFeedQueryRequestStart(long timestamp) {}

    /**
     * Log before filling out and serializing a feed actions upload request. Starts the network
     * request flow.
     * @param timestamp Event time.
     */
    default void logActionsUploadRequestStart(long timestamp) {}

    /**
     * Log before filling out and serializing a web feed request for all followed web feeds. Starts
     * the network request flow.
     */
    default void logWebFeedRequestStart(long timestamp) {}

    /**
     * Log before filling out and serializing a web feed request for a single web feed, used by the
     * cormorant surface. Starts the network request flow.
     */
    default void logSingleWebFeedRequestStart(long timestamp) {}

    /**
     * Log after the request has been sent.
     * @param timestamp Event time.
     */
    default void logRequestSent(long timestamp) {}

    /**
     * Log after the response is received and before it is parsed.
     * @param serverRecvTimestamp Server-reported time (nanoseconds) at which the request arrived.
     * @param serverSendTimestamp Server-reported time (nanoseconds) at which the response was sent.
     * @param clientRecvTimestamp Client time at which the response was received.
     */
    default void logResponseReceived(
            long serverRecvTimestamp, long serverSendTimestamp, long clientRecvTimestamp) {}

    /**
     * Log after logResponseReceived() if there's a network error, or after parsing the response
     * otherwise. Ends the network request flow.
     * @param timestamp Event time.
     * @param canonicalStatus Network request status code. See
     *         //third_party/abseil-cpp/absl/status/status.h.
     */
    default void logRequestFinished(long timestamp, int canonicalStatus) {}
}
