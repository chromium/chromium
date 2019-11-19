// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.net.RequestFinishedInfo;

import java.util.Date;

/**
 * Implementation of {@link RequestFinishedInfo.Metrics}.
 */
@VisibleForTesting
public final class CronetMetrics extends RequestFinishedInfo.Metrics {
    private final long mRequestStartMs;
    private final long mDnsStartMs;
    private final long mDnsEndMs;
    private final long mConnectStartMs;
    private final long mConnectEndMs;
    private final long mSslStartMs;
    private final long mSslEndMs;
    private final long mSendingStartMs;
    private final long mSendingEndMs;
    private final long mPushStartMs;
    private final long mPushEndMs;
    private final long mResponseStartMs;
    private final long mRequestEndMs;
    private final boolean mSocketReused;

    // TODO(mgersh): Delete after the switch to the new API http://crbug.com/629194
    @Nullable
    private final Long mTtfbMs;
    // TODO(mgersh): Delete after the switch to the new API http://crbug.com/629194
    @Nullable
    private final Long mTotalTimeMs;
    @Nullable
    private final Long mSentByteCount;
    @Nullable
    private final Long mReceivedByteCount;

    @Nullable
    private static Date toDate(long timestamp) {
        if (timestamp != -1) {
            return new Date(timestamp);
        }
        return null;
    }

    private static boolean checkOrder(long start, long end) {
        // If end doesn't exist, start can be anything, including also not existing
        // If end exists, start must also exist and be before end
        return (end >= start && start != -1) || end == -1;
    }

    /**
     * Old-style constructor
     * TODO(mgersh): Delete after the switch to the new API http://crbug.com/629194
     */
    public CronetMetrics(@Nullable Long ttfbMs, @Nullable Long totalTimeMs,
            @Nullable Long sentByteCount, @Nullable Long receivedByteCount) {
        mTtfbMs = ttfbMs;
        mTotalTimeMs = totalTimeMs;
        mSentByteCount = sentByteCount;
        mReceivedByteCount = receivedByteCount;

        // Everything else is -1 (translates to null) for now
        mRequestStartMs = -1;
        mDnsStartMs = -1;
        mDnsEndMs = -1;
        mConnectStartMs = -1;
        mConnectEndMs = -1;
        mSslStartMs = -1;
        mSslEndMs = -1;
        mSendingStartMs = -1;
        mSendingEndMs = -1;
        mPushStartMs = -1;
        mPushEndMs = -1;
        mResponseStartMs = -1;
        mRequestEndMs = -1;
        mSocketReused = false;
    }

    /**
     * New-style constructor
     */
    public CronetMetrics(long requestStartMs, long dnsStartMs, long dnsEndMs, long connectStartMs,
            long connectEndMs, long sslStartMs, long sslEndMs, long sendingStartMs,
            long sendingEndMs, long pushStartMs, long pushEndMs, long responseStartMs,
            long requestEndMs, boolean socketReused, long sentByteCount, long receivedByteCount) {
        // Check that no end times are before corresponding start times,
        // or exist when start time doesn't.
        assert checkOrder(dnsStartMs, dnsEndMs);
        assert checkOrder(connectStartMs, connectEndMs);
        assert checkOrder(sslStartMs, sslEndMs);
        assert checkOrder(sendingStartMs, sendingEndMs);
        assert checkOrder(pushStartMs, pushEndMs);
        // requestEnd always exists, so just check that it's after start
        assert requestEndMs >= responseStartMs;
        // Spot-check some of the other orderings
        assert dnsStartMs >= requestStartMs || dnsStartMs == -1;
        assert sendingStartMs >= requestStartMs || sendingStartMs == -1;
        assert sslStartMs >= connectStartMs || sslStartMs == -1;
        assert responseStartMs >= sendingStartMs || responseStartMs == -1;
        mRequestStartMs = requestStartMs;
        mDnsStartMs = dnsStartMs;
        mDnsEndMs = dnsEndMs;
        mConnectStartMs = connectStartMs;
        mConnectEndMs = connectEndMs;
        mSslStartMs = sslStartMs;
        mSslEndMs = sslEndMs;
        mSendingStartMs = sendingStartMs;
        mSendingEndMs = sendingEndMs;
        mPushStartMs = pushStartMs;
        mPushEndMs = pushEndMs;
        mResponseStartMs = responseStartMs;
        mRequestEndMs = requestEndMs;
        mSocketReused = socketReused;
        mSentByteCount = sentByteCount;
        mReceivedByteCount = receivedByteCount;

        // TODO(mgersh): delete these after embedders stop using them http://crbug.com/629194
        if (requestStartMs != -1 && responseStartMs != -1) {
            mTtfbMs = responseStartMs - requestStartMs;
        } else {
            mTtfbMs = null;
        }
        if (requestStartMs != -1 && requestEndMs != -1) {
            mTotalTimeMs = requestEndMs - requestStartMs;
        } else {
            mTotalTimeMs = null;
        }
    }

    @Nullable
    @Override
    public Date getRequestStart() {
        return toDate(mRequestStartMs);
    }

    @Nullable
    @Override
    public Date getDnsStart() {
        return toDate(mDnsStartMs);
    }

    @Nullable
    @Override
    public Date getDnsEnd() {
        return toDate(mDnsEndMs);
    }

    @Nullable
    @Override
    public Date getConnectStart() {
        return toDate(mConnectStartMs);
    }

    @Nullable
    @Override
    public Date getConnectEnd() {
        return toDate(mConnectEndMs);
    }

    @Nullable
    @Override
    public Date getSslStart() {
        return toDate(mSslStartMs);
    }

    @Nullable
    @Override
    public Date getSslEnd() {
        return toDate(mSslEndMs);
    }

    @Nullable
    @Override
    public Date getSendingStart() {
        return toDate(mSendingStartMs);
    }

    @Nullable
    @Override
    public Date getSendingEnd() {
        return toDate(mSendingEndMs);
    }

    @Nullable
    @Override
    public Date getPushStart() {
        return toDate(mPushStartMs);
    }

    @Nullable
    @Override
    public Date getPushEnd() {
        return toDate(mPushEndMs);
    }

    @Nullable
    @Override
    public Date getResponseStart() {
        return toDate(mResponseStartMs);
    }

    @Nullable
    @Override
    public Date getRequestEnd() {
        return toDate(mRequestEndMs);
    }

    @Override
    public boolean getSocketReused() {
        return mSocketReused;
    }

    @Nullable
    @Override
    public Long getTtfbMs() {
        return mTtfbMs;
    }

    @Nullable
    @Override
    public Long getTotalTimeMs() {
        return mTotalTimeMs;
    }

    @Nullable
    @Override
    public Long getSentByteCount() {
        return mSentByteCount;
    }

    @Nullable
    @Override
    public Long getReceivedByteCount() {
        return mReceivedByteCount;
    }
}
