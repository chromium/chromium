// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

/**
 * Interface for logging latency and availability signals for feed network requests. All timestamps
 * are in terms of nanoseconds since system boot.
 *
 * See {@link FeedLaunchReliabilityLogger} for the network request start event methods: they start
 * the network request flow and return FeedNetworkRequestReliabilityLogger instances.
 */
public interface FeedNetworkRequestReliabilityLogger {
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
     * otherwise.
     * @param timestamp Event time.
     * @param canonicalStatus Network request status code. See
     *         //third_party/abseil-cpp/absl/status/status.h.
     */
    default void logRequestFinished(long timestamp, int canonicalStatus) {}
}