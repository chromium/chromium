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
class AndroidUrlRequestWrapper extends org.chromium.net.ExperimentalUrlRequest {
    private final android.net.http.UrlRequest mBackend;
    private final AndroidHttpEngineWrapper mEngine;
    private final String mInitialUrl;
    private final Collection<Object> mAnnotations;
    private final VersionSafeCallbacks.RequestFinishedInfoListener mRequestFinishedInfoListener;

    AndroidUrlRequestWrapper(
            android.net.http.UrlRequest backend,
            AndroidHttpEngineWrapper engine,
            String url,
            Collection<Object> annotations,
            RequestFinishedInfo.Listener requestFinishedInfoListener) {
        this.mBackend = backend;
        this.mEngine = engine;
        this.mInitialUrl = url;
        this.mAnnotations = annotations;
        this.mRequestFinishedInfoListener =
                requestFinishedInfoListener == null
                        ? null
                        : new VersionSafeCallbacks.RequestFinishedInfoListener(
                                requestFinishedInfoListener);
    }

    @Override
    public void start() {
        mBackend.start();
    }

    @Override
    public void followRedirect() {
        mBackend.followRedirect();
    }

    @Override
    public void read(ByteBuffer buffer) {
        mBackend.read(buffer);
    }

    @Override
    public void cancel() {
        mBackend.cancel();
    }

    @Override
    public boolean isDone() {
        return mBackend.isDone();
    }

    @Override
    public void getStatus(StatusListener listener) {
        mBackend.getStatus(new AndroidUrlRequestStatusListenerWrapper(listener));
    }

    /**
     * Creates an {@link AndroidUrlRequestWrapper} that is recorded on the callback.
     *
     * @param backend the http UrlRequest
     * @param callback the stream's callback
     * @return the wrapped request
     */
    static AndroidUrlRequestWrapper createAndAddToCallback(
            android.net.http.UrlRequest backend,
            AndroidUrlRequestCallbackWrapper callback,
            AndroidHttpEngineWrapper engine,
            String url,
            Collection<Object> annotations,
            RequestFinishedInfo.Listener requestFinishedInfoListener) {
        AndroidUrlRequestWrapper wrappedRequest =
                new AndroidUrlRequestWrapper(
                        backend, engine, url, annotations, requestFinishedInfoListener);
        callback.setRequest(wrappedRequest);
        return wrappedRequest;
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
                mEngine,
                mInitialUrl,
                mAnnotations,
                mRequestFinishedInfoListener,
                finishedReason,
                responseInfo,
                exception);
    }
}
