// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import androidx.annotation.RequiresApi;
import androidx.annotation.RequiresExtension;

import org.chromium.net.CronetEngine;
import org.chromium.net.CronetException;
import org.chromium.net.RequestFinishedInfo.Listener;

import java.nio.ByteBuffer;
import java.time.Duration;
import java.util.Collection;

// Note we specify both RequiresApi and RequiresExtension because some older linters may only
// recognize the former.
@RequiresApi(EXT_API_LEVEL)
@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidBidirectionalStreamWrapper extends org.chromium.net.ExperimentalBidirectionalStream {
    private final android.net.http.BidirectionalStream mBackend;
    private final AndroidHttpEngineWrapper mEngine;
    private final String mInitialUrl;
    private final Collection<Object> mAnnotations;
    private volatile long mRequestStartTimeNanos;
    private volatile Duration mTimeToResponseStart;

    AndroidBidirectionalStreamWrapper(
            android.net.http.BidirectionalStream backend,
            AndroidHttpEngineWrapper engine,
            String url,
            Collection<Object> annotations) {
        this.mBackend = backend;
        this.mEngine = engine;
        this.mInitialUrl = url;
        this.mAnnotations = annotations;
    }

    @Override
    public void start() {
        mRequestStartTimeNanos = System.nanoTime();
        mBackend.start();
    }

    @Override
    public void read(ByteBuffer buffer) {
        mBackend.read(buffer);
    }

    @Override
    public void write(ByteBuffer buffer, boolean endOfStream) {
        mBackend.write(buffer, endOfStream);
    }

    @Override
    public void flush() {
        mBackend.flush();
    }

    @Override
    public void cancel() {
        mBackend.cancel();
    }

    @Override
    public boolean isDone() {
        return mBackend.isDone();
    }

    /**
     * Creates an {@link AndroidUrlRequestWrapper} that is stored on the callback.
     *
     * @param backend the http UrlRequest
     * @param callback the stream's callback
     * @return the wrapped request
     */
    static AndroidBidirectionalStreamWrapper createAndAddToCallback(
            android.net.http.BidirectionalStream backend,
            AndroidBidirectionalStreamCallbackWrapper callback,
            AndroidHttpEngineWrapper engine,
            String url,
            Collection<Object> annotations) {
        AndroidBidirectionalStreamWrapper wrappedStream =
                new AndroidBidirectionalStreamWrapper(backend, engine, url, annotations);
        callback.setStream(wrappedStream);
        return wrappedStream;
    }

    void recordResponseHeadersReceived() {
        mTimeToResponseStart = Duration.ofNanos(System.nanoTime() - mRequestStartTimeNanos);
    }

    /**
     * HttpEngine does not support {@link CronetEngine#addRequestFinishedListener(Listener)}). To
     * preserve compatibility with Cronet users who use that method, we reimplement the
     * functionality using {@link android.net.http.BidirectionalStream#getFinishedRequestTimings()}
     * when available (or a placeholder CronetMetrics object on older SDK extensions).
     */
    void maybeReportMetrics(
            @RequestFinishedInfoImpl.FinishedReason int finishedReason,
            AndroidUrlResponseInfoWrapper responseInfo,
            CronetException exception) {
        AndroidRequestFinishedInfoWrapper.reportFinished(
                mEngine,
                mInitialUrl,
                mAnnotations,
                null,
                finishedReason,
                responseInfo,
                exception,
                AndroidRequestFinishedInfoWrapper.isFinishedRequestTimingsSupported()
                        ? mBackend.getFinishedRequestTimings()
                        : null,
                mTimeToResponseStart);
    }
}
