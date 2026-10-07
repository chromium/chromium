// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.os.Build;
import android.os.Process;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.chromium.net.ConnectionCloseSource;
import org.chromium.net.CronetException;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.impl.CronetLogger.CronetTrafficInfo;
import org.chromium.net.impl.CronetLogger.CronetTrafficInfo.RequestTerminalState;
import org.chromium.net.impl.RequestFinishedInfoImpl.FinishedReason;

import java.time.Duration;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * A random assortment of utilities for factoring out commonalities between CronetUrlRequest and
 * CronetBidirectionalStream implementations.
 */
final class CronetRequestCommon {
    private CronetRequestCommon() {}

    public static RequestTerminalState finishedReasonToCronetTrafficInfoRequestTerminalState(
            @FinishedReason int finishedReason) {
        switch (finishedReason) {
            case RequestFinishedInfo.SUCCEEDED:
                return RequestTerminalState.SUCCEEDED;
            case RequestFinishedInfo.FAILED:
                return RequestTerminalState.ERROR;
            case RequestFinishedInfo.CANCELED:
                return RequestTerminalState.CANCELLED;
            default:
                throw new IllegalArgumentException(
                        "Invalid finished reason while producing request terminal state: "
                                + finishedReason);
        }
    }

    /**
     * Estimates the byte size of the headers in their on-wire format. We are not really interested
     * in their specific size but something which is close enough.
     */
    public static long estimateHeadersSizeInBytes(Map<String, List<String>> headers) {
        if (headers == null) return 0;

        long responseHeaderSizeInBytes = 0;
        for (Map.Entry<String, List<String>> entry : headers.entrySet()) {
            String key = entry.getKey();
            if (key != null) responseHeaderSizeInBytes += key.length();
            if (entry.getValue() == null) continue;

            for (String content : entry.getValue()) {
                responseHeaderSizeInBytes += content.length();
            }
        }
        return responseHeaderSizeInBytes;
    }

    /**
     * Estimates the byte size of the headers in their on-wire format. We are not really interested
     * in their specific size but something which is close enough.
     */
    public static long estimateHeadersSizeInBytes(Collection<Map.Entry<String, String>> headers) {
        if (headers == null) return 0;
        long responseHeaderSizeInBytes = 0;
        for (Map.Entry<String, String> entry : headers) {
            String key = entry.getKey();
            if (key != null) responseHeaderSizeInBytes += key.length();
            String value = entry.getValue();
            if (value != null) responseHeaderSizeInBytes += entry.getValue().length();
        }
        return responseHeaderSizeInBytes;
    }

    /**
     * Estimates the byte size of the headers in their on-wire format. We are not really interested
     * in their specific size but something which is close enough.
     */
    public static long estimateHeadersSizeInBytes(String[] headers) {
        if (headers == null) return 0;
        long responseHeaderSizeInBytes = 0;
        for (var entry : headers) {
            if (entry != null) responseHeaderSizeInBytes += entry.length();
        }
        return responseHeaderSizeInBytes;
    }

    /**
     * Calculates goodput in bytes per second.
     *
     * @param bytes the number of payload bytes transferred
     * @param durationMicros the duration of the transfer in microseconds
     * @return the goodput in bytes per second, or -1 if bytes or duration are invalid
     */
    public static long calculateGoodputBytesPerSec(long bytes, long durationMicros) {
        if (bytes <= 0 || durationMicros <= 0) {
            return -1;
        }
        return (bytes * 1_000_000L) / durationMicros;
    }

    @RequiresApi(Build.VERSION_CODES.O)
    public static CronetTrafficInfo buildCronetTrafficInfo(
            @NonNull CronetMetrics metrics,
            boolean isBidiStream,
            boolean isAdaptiveNetworkStream,
            @FinishedReason int finishedReason,
            long requestHeaderSizeInBytes,
            long requestBodySizeInBytes,
            long responseBodySizeInBytes,
            int readCount,
            int uploadReadCount,
            @Nullable UrlResponseInfoImpl responseInfo,
            @Nullable CronetException exception,
            boolean quicConnectionMigrationAttempted,
            boolean quicConnectionMigrationSuccessful,
            int nonfinalUserCallbackExceptionCount,
            boolean finalUserCallbackThrew) {
        assert metrics != null;

        // Most of the CronetTrafficInfo fields have similar names/semantics. To avoid bugs due to
        // typos everything is final, this means that things have to initialized through an if/else.
        final Map<String, List<String>> responseHeaders;
        final String negotiatedProtocol;
        final int httpStatusCode;
        final CronetTrafficInfo.CacheState cacheState;
        final Boolean isProxied = responseInfo != null ? responseInfo.isProxied() : null;
        if (responseInfo != null) {
            responseHeaders = responseInfo.getAllHeaders();
            negotiatedProtocol = responseInfo.getNegotiatedProtocol();
            httpStatusCode = responseInfo.getHttpStatusCode();
            cacheState =
                    responseInfo.wasCached()
                            ? CronetTrafficInfo.CacheState.CACHE_HIT
                            : CronetTrafficInfo.CacheState.NOT_CACHED;
        } else {
            responseHeaders = Collections.emptyMap();
            negotiatedProtocol = "";
            httpStatusCode = 0;
            cacheState = CronetTrafficInfo.CacheState.UNSPECIFIED;
        }

        final long responseHeaderSizeInBytes = estimateHeadersSizeInBytes(responseHeaders);

        final Duration totalLatency;
        if (metrics.getRequestStart() != null && metrics.getRequestEnd() != null) {
            totalLatency =
                    Duration.ofMillis(
                            metrics.getRequestEnd().getTime()
                                    - metrics.getRequestStart().getTime());
        } else {
            totalLatency = Duration.ofSeconds(0);
        }

        int networkInternalErrorCode = 0;
        int quicNetworkErrorCode = 0;
        @ConnectionCloseSource int source = ConnectionCloseSource.UNKNOWN;
        CronetTrafficInfo.RequestFailureReason failureReason =
                CronetTrafficInfo.RequestFailureReason.UNKNOWN;

        // Going through the API layer will lead to NoSuchMethodError exceptions
        // because there is no guarantee that the API will have the method.
        // It's possible to use an old API of Cronet with a new implementation.
        // In order to work around this, only impl classes are mentioned
        // to ensure that the methods will always be found.
        // See b/361725824 for more information.
        if (exception instanceof NetworkExceptionImpl networkException) {
            networkInternalErrorCode = networkException.getCronetInternalErrorCode();
            failureReason = CronetTrafficInfo.RequestFailureReason.NETWORK;
        } else if (exception instanceof QuicExceptionImpl quicException) {
            networkInternalErrorCode = quicException.getCronetInternalErrorCode();
            quicNetworkErrorCode = quicException.getQuicDetailedErrorCode();
            source = quicException.getConnectionCloseSource();
            failureReason = CronetTrafficInfo.RequestFailureReason.NETWORK;
        } else if (exception != null) {
            failureReason = CronetTrafficInfo.RequestFailureReason.OTHER;
        }

        return new CronetTrafficInfo(
                requestHeaderSizeInBytes,
                requestBodySizeInBytes,
                responseHeaderSizeInBytes,
                responseBodySizeInBytes,
                httpStatusCode,
                totalLatency,
                negotiatedProtocol,
                quicConnectionMigrationAttempted,
                quicConnectionMigrationSuccessful,
                finishedReasonToCronetTrafficInfoRequestTerminalState(finishedReason),
                nonfinalUserCallbackExceptionCount,
                readCount,
                uploadReadCount,
                isBidiStream,
                finalUserCallbackThrew,
                Process.myUid(),
                networkInternalErrorCode,
                quicNetworkErrorCode,
                source,
                failureReason,
                metrics.getSocketReused(),
                ImplVersion.getCronetVersion(),
                NativeCronetEngineBuilderImpl.getCronetSource(),
                metrics.getDnsDurationInMicroseconds(),
                metrics.getSSLDurationInMicroseconds(),
                metrics.getConnectDurationInMicroseconds(),
                metrics.getTimeToWriteFirstByteInMicroseconds(),
                metrics.getTimeToReceiveHeaderLastByteMicroseconds(),
                isProxied,
                isAdaptiveNetworkStream,
                cacheState,
                calculateGoodputBytesPerSec(
                        requestBodySizeInBytes, metrics.getSendingDurationInMicroseconds()),
                calculateGoodputBytesPerSec(
                        responseBodySizeInBytes, metrics.getReceivingDurationInMicroseconds()));
    }
}
