// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.impl.CronetLogger.CronetTrafficInfo.RequestTerminalState;
import org.chromium.net.impl.RequestFinishedInfoImpl.FinishedReason;

import java.util.Collection;
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
}
