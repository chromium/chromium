// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import androidx.annotation.RequiresExtension;

import org.chromium.net.CronetEngine;
import org.chromium.net.CronetException;
import org.chromium.net.RequestFinishedInfo;
import org.chromium.net.RequestFinishedInfo.Listener;

import java.nio.ByteBuffer;
import java.util.Collection;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidBidirectionalStreamWrapper extends org.chromium.net.ExperimentalBidirectionalStream {
    private final android.net.http.BidirectionalStream mBackend;
    private final AndroidHttpEngineWrapper mEngine;
    private final String mInitialUrl;
    private final Collection<Object> mAnnotations;

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

    /**
     * HttpEngine does not support {@link CronetEngine#addRequestFinishedListener(Listener)}). To
     * preserve compatibility with Cronet users who use that method, we reimplement the
     * functionality with a placeholder CronetMetric object. This reimplementation allows for {@link
     * RequestFinishedInfo.Listener#onRequestFinished(RequestFinishedInfo)} to be called so as not
     * to unknowingly block users who might be depending on that API call.
     */
    void maybeReportMetrics(
            @RequestFinishedInfoImpl.FinishedReason int finishedReason,
            AndroidUrlResponseInfoWrapper responseInfo,
            CronetException exception) {
        AndroidRequestFinishedInfoWrapper.reportFinished(
                mEngine, mInitialUrl, mAnnotations, null, finishedReason, responseInfo, exception);
    }
}
