// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import android.net.http.FinishedRequestTimings;
import android.os.Build;
import android.os.ext.SdkExtensions;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.RequiresExtension;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.net.CronetException;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.UrlResponseInfo;
import org.chromium.net.impl.VersionSafeCallbacks.RequestFinishedInfoListener;

import java.time.Duration;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.Collection;

/** Wrapper for {@link RequestFinishedInfo} in {@link HttpEngineNativeProvider}. */
@VisibleForTesting
public class AndroidRequestFinishedInfoWrapper extends RequestFinishedInfoImpl {
    private static final String TAG = RequestFinishedInfoImpl.class.getSimpleName();
    private static final int FINISHED_REQUEST_TIMINGS_EXT_VERSION = 23;

    private static boolean sListenerSupportLimitedLogged;

    private AndroidRequestFinishedInfoWrapper(
            String url,
            Collection<Object> annotations,
            Metrics metrics,
            int finishedReason,
            @Nullable UrlResponseInfo responseInfo,
            @Nullable CronetException exception) {
        super(url, annotations, metrics, finishedReason, responseInfo, exception);
    }

    /**
     * Returns whether {@link android.net.http.FinishedRequestTimings} is supported by the
     * underlying {@link android.net.http.HttpEngine} implementation on the current device.
     */
    public static boolean isFinishedRequestTimingsSupported() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                && SdkExtensions.getExtensionVersion(EXT_API_LEVEL)
                        >= FINISHED_REQUEST_TIMINGS_EXT_VERSION;
    }

    @Override
    public Metrics getMetrics() {
        if (!isFinishedRequestTimingsSupported() && !sListenerSupportLimitedLogged) {
            Log.i(
                    TAG,
                    "RequestFinishedInfo.getMetrics() is unsupported when HttpEngineNativeProvider"
                            + " is used. The Metrics object will return null values.");
            sListenerSupportLimitedLogged = true;
        }
        return super.getMetrics();
    }

    @RequiresApi(EXT_API_LEVEL)
    @RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
    static void reportFinished(
            AndroidHttpEngineWrapper engine,
            String url,
            Collection<Object> annotations,
            RequestFinishedInfoListener listener,
            @RequestFinishedInfoImpl.FinishedReason int finishedReason,
            UrlResponseInfo responseInfo,
            CronetException exception,
            @Nullable FinishedRequestTimings timings,
            @Nullable Duration timeToResponseStart) {
        RequestFinishedInfo requestInfo =
                new AndroidRequestFinishedInfoWrapper(
                        url,
                        annotations,
                        timings == null
                                ? CronetMetrics.empty()
                                : fromFinishedRequestTimings(timings, timeToResponseStart),
                        finishedReason,
                        responseInfo,
                        exception);
        engine.reportRequestFinished(requestInfo, listener);
    }

    @RequiresApi(EXT_API_LEVEL)
    @RequiresExtension(extension = EXT_API_LEVEL, version = FINISHED_REQUEST_TIMINGS_EXT_VERSION)
    private static Metrics fromFinishedRequestTimings(
            FinishedRequestTimings timings, @Nullable Duration timeToResponseStart) {
        Instant requestStart = timings.getRequestStart();
        long sendingStartMicros = instantToMicros(timings.getSendingStart());
        long requestEndMicros = instantToMicros(timings.getRequestEnd());
        long responseStartMicros =
                (requestStart != null && timeToResponseStart != null)
                        ? instantToMicros(requestStart.plus(timeToResponseStart))
                        : -1;
        if (responseStartMicros != -1 && requestEndMicros != -1) {
            // timeToResponseStart is measured on the calling/callback threads, whereas
            // FinishedRequestTimings timestamps are captured on the native network thread. Clamp
            // to [sendingStart, requestEnd] so CronetMetrics ordering invariants hold despite
            // callback scheduling delays.
            if (sendingStartMicros != -1) {
                responseStartMicros = Math.max(responseStartMicros, sendingStartMicros);
            }
            responseStartMicros = Math.min(responseStartMicros, requestEndMicros);
        } else {
            responseStartMicros = -1;
        }
        return new CronetMetrics(
                instantToMicros(requestStart),
                instantToMicros(timings.getDnsStart()),
                instantToMicros(timings.getDnsEnd()),
                instantToMicros(timings.getConnectingStart()),
                instantToMicros(timings.getConnectingEnd()),
                instantToMicros(timings.getTlsHandshakeStart()),
                instantToMicros(timings.getTlsHandshakeEnd()),
                sendingStartMicros,
                instantToMicros(timings.getSendingEnd()),
                /* pushStartMicroseconds= */ -1,
                /* pushEndMicroseconds= */ -1,
                responseStartMicros,
                requestEndMicros,
                timings.wasSocketReused(),
                timings.getSentByteCount(),
                timings.getReceivedByteCount());
    }

    @RequiresApi(EXT_API_LEVEL)
    private static long instantToMicros(@Nullable Instant instant) {
        return instant == null ? -1 : ChronoUnit.MICROS.between(Instant.EPOCH, instant);
    }
}
