// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.net.RequestFinishedInfo;

import java.util.Date;
import java.util.concurrent.TimeUnit;

/** Implementation of {@link RequestFinishedInfo.Metrics}. */
@VisibleForTesting
@JNINamespace("cronet")
public final class CronetMetrics extends RequestFinishedInfo.Metrics {
    private final long mRequestStartMicroseconds;
    private final long mDnsStartMicroseconds;
    private final long mDnsEndMicroseconds;
    private final long mConnectStartMicroseconds;
    private final long mConnectEndMicroseconds;
    private final long mSslStartMicroseconds;
    private final long mSslEndMicroseconds;
    private final long mSendingStartMicroseconds;
    private final long mSendingEndMicroseconds;
    private final long mPushStartMicroseconds;
    private final long mPushEndMicroseconds;
    private final long mResponseStartMicroseconds;
    private final long mRequestEndMicroseconds;
    private final boolean mSocketReused;

    // TODO(mgersh): Delete after the switch to the new API http://crbug.com/629194
    @Nullable private final Long mTtfbMicroseconds;
    // TODO(mgersh): Delete after the switch to the new API http://crbug.com/629194
    @Nullable private final Long mTotalTimeMicroseconds;
    @Nullable private final Long mSentByteCount;
    @Nullable private final Long mReceivedByteCount;

    @Nullable
    private static Date toDate(long timestamp) {
        if (timestamp != -1) {
            return new Date(TimeUnit.MICROSECONDS.toMillis(timestamp));
        }
        return null;
    }

    static long getDateDeltaMillisOrDefault(Date before, Date after, long defaultValue) {
        if (before == null || after == null) return defaultValue;
        return after.getTime() - before.getTime();
    }

    static long getDurationBetweenTimestampsInMicros(long before, long after) {
        if (before == -1 || after == -1) {
            return -1;
        }
        return after - before;
    }

    private static boolean checkOrder(long start, long end) {
        // If end doesn't exist, start can be anything, including also not existing
        // If end exists, start must also exist and be before end
        return (end >= start && start != -1) || end == -1;
    }

    /**
     * Returns a metrics object populated with empty values.
     *
     * <p>Ideally we should just provide Cronet users with a null Metrics object instead, but sadly,
     * for historical reasons not all users handle a null object properly.
     */
    public static CronetMetrics empty() {
        return new CronetMetrics(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, false, 0, 0);
    }

    /** New-style constructor */
    @CalledByNative
    public CronetMetrics(
            long requestStartMicroseconds,
            long dnsStartMicroseconds,
            long dnsEndMicroseconds,
            long connectStartMicroseconds,
            long connectEndMicroseconds,
            long sslStartMicroseconds,
            long sslEndMicroseconds,
            long sendingStartMicroseconds,
            long sendingEndMicroseconds,
            long pushStartMicroseconds,
            long pushEndMicroseconds,
            long responseStartMicroseconds,
            long requestEndMicroseconds,
            boolean socketReused,
            long sentByteCount,
            long receivedByteCount) {
        // Check that no end times are before corresponding start times,
        // or exist when start time doesn't.
        assert checkOrder(dnsStartMicroseconds, dnsEndMicroseconds);
        assert checkOrder(connectStartMicroseconds, connectEndMicroseconds);
        assert checkOrder(sslStartMicroseconds, sslEndMicroseconds);
        assert checkOrder(sendingStartMicroseconds, sendingEndMicroseconds);
        assert checkOrder(pushStartMicroseconds, pushEndMicroseconds);
        // requestEnd always exists, so just check that it's after start
        assert requestEndMicroseconds >= responseStartMicroseconds;
        // Spot-check some of the other orderings
        assert dnsStartMicroseconds >= requestStartMicroseconds || dnsStartMicroseconds == -1;
        assert sendingStartMicroseconds >= requestStartMicroseconds
                || sendingStartMicroseconds == -1;
        assert sslStartMicroseconds >= connectStartMicroseconds || sslStartMicroseconds == -1;
        assert responseStartMicroseconds >= sendingStartMicroseconds
                || responseStartMicroseconds == -1;
        mRequestStartMicroseconds = requestStartMicroseconds;
        mDnsStartMicroseconds = dnsStartMicroseconds;
        mDnsEndMicroseconds = dnsEndMicroseconds;
        mConnectStartMicroseconds = connectStartMicroseconds;
        mConnectEndMicroseconds = connectEndMicroseconds;
        mSslStartMicroseconds = sslStartMicroseconds;
        mSslEndMicroseconds = sslEndMicroseconds;
        mSendingStartMicroseconds = sendingStartMicroseconds;
        mSendingEndMicroseconds = sendingEndMicroseconds;
        mPushStartMicroseconds = pushStartMicroseconds;
        mPushEndMicroseconds = pushEndMicroseconds;
        mResponseStartMicroseconds = responseStartMicroseconds;
        mRequestEndMicroseconds = requestEndMicroseconds;
        mSocketReused = socketReused;
        mSentByteCount = sentByteCount;
        mReceivedByteCount = receivedByteCount;

        // TODO(mgersh): delete these after embedders stop using them http://crbug.com/629194
        if (requestStartMicroseconds != -1 && responseStartMicroseconds != -1) {
            mTtfbMicroseconds = responseStartMicroseconds - requestStartMicroseconds;
        } else {
            mTtfbMicroseconds = null;
        }
        if (requestStartMicroseconds != -1 && requestEndMicroseconds != -1) {
            mTotalTimeMicroseconds = requestEndMicroseconds - requestStartMicroseconds;
        } else {
            mTotalTimeMicroseconds = null;
        }
    }

    @Nullable
    @Override
    public Date getRequestStart() {
        return toDate(mRequestStartMicroseconds);
    }

    @Nullable
    @Override
    public Date getDnsStart() {
        return toDate(mDnsStartMicroseconds);
    }

    @Nullable
    @Override
    public Date getDnsEnd() {
        return toDate(mDnsEndMicroseconds);
    }

    @Nullable
    @Override
    public Date getConnectStart() {
        return toDate(mConnectStartMicroseconds);
    }

    @Nullable
    @Override
    public Date getConnectEnd() {
        return toDate(mConnectEndMicroseconds);
    }

    @Nullable
    @Override
    public Date getSslStart() {
        return toDate(mSslStartMicroseconds);
    }

    @Nullable
    @Override
    public Date getSslEnd() {
        return toDate(mSslEndMicroseconds);
    }

    @Nullable
    @Override
    public Date getSendingStart() {
        return toDate(mSendingStartMicroseconds);
    }

    @Nullable
    @Override
    public Date getSendingEnd() {
        return toDate(mSendingEndMicroseconds);
    }

    @Nullable
    @Override
    public Date getPushStart() {
        return toDate(mPushStartMicroseconds);
    }

    @Nullable
    @Override
    public Date getPushEnd() {
        return toDate(mPushEndMicroseconds);
    }

    @Nullable
    @Override
    public Date getResponseStart() {
        return toDate(mResponseStartMicroseconds);
    }

    @Nullable
    @Override
    public Date getRequestEnd() {
        return toDate(mRequestEndMicroseconds);
    }

    @Override
    public boolean getSocketReused() {
        return mSocketReused;
    }

    @Nullable
    @Override
    public Long getTtfbMs() {
        return mTtfbMicroseconds == null ? null : TimeUnit.MICROSECONDS.toMillis(mTtfbMicroseconds);
    }

    @Nullable
    @Override
    public Long getTotalTimeMs() {
        return mTotalTimeMicroseconds == null
                ? null
                : TimeUnit.MICROSECONDS.toMillis(mTotalTimeMicroseconds);
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

    public long getRequestStartMicroseconds() {
        return mRequestStartMicroseconds;
    }

    public long getDnsStartMicroseconds() {
        return mDnsStartMicroseconds;
    }

    public long getDnsEndMicroseconds() {
        return mDnsEndMicroseconds;
    }

    public long getConnectStartMicroseconds() {
        return mConnectStartMicroseconds;
    }

    public long getConnectEndMicroseconds() {
        return mConnectEndMicroseconds;
    }

    public long getSslStartMicroseconds() {
        return mSslStartMicroseconds;
    }

    public long getSslEndMicroseconds() {
        return mSslEndMicroseconds;
    }

    public long getSendingStartMicroseconds() {
        return mSendingStartMicroseconds;
    }

    public long getSendingEndMicroseconds() {
        return mSendingEndMicroseconds;
    }

    // Package-private as we don't want to expose these in the public Cronet API, for now. These
    // return long, not Date, because we want to preserve the microsecond precision (Date is
    // millisecond precision).
    long getDnsDurationInMicroseconds() {
        return getDurationBetweenTimestampsInMicros(mDnsStartMicroseconds, mDnsEndMicroseconds);
    }

    // Package-private as we don't want to expose these in the public Cronet API, for now. These
    // return long, not Date, because we want to preserve the microsecond precision (Date is
    // millisecond precision).
    long getSSLDurationInMicroseconds() {
        return getDurationBetweenTimestampsInMicros(mSslStartMicroseconds, mSslEndMicroseconds);
    }

    // Package-private as we don't want to expose these in the public Cronet API, for now. These
    // return long, not Date, because we want to preserve the microsecond precision (Date is
    // millisecond precision).
    long getConnectDurationInMicroseconds() {
        return getDurationBetweenTimestampsInMicros(
                mConnectStartMicroseconds, mConnectEndMicroseconds);
    }

    // Package-private as we don't want to expose these in the public Cronet API, for now. These
    // return long, not Date, because we want to preserve the microsecond precision (Date is
    // millisecond precision).
    long getTimeToWriteFirstByteInMicroseconds() {
        return getDurationBetweenTimestampsInMicros(
                mRequestStartMicroseconds, mSendingStartMicroseconds);
    }

    // Package-private as we don't want to expose these in the public Cronet API, for now. These
    // return long, not Date, because we want to preserve the microsecond precision (Date is
    // millisecond precision).
    long getTimeToReceiveHeaderLastByteMicroseconds() {
        return getDurationBetweenTimestampsInMicros(
                mRequestStartMicroseconds, mResponseStartMicroseconds);
    }
}
