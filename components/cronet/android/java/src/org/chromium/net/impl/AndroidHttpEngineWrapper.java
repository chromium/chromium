// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.net.http.HttpEngine;

import androidx.annotation.RequiresApi;

import org.chromium.net.ExperimentalCronetEngine;

import java.io.IOException;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandlerFactory;
import java.util.concurrent.Executor;

@RequiresApi(api = 34)
class AndroidHttpEngineWrapper extends ExperimentalCronetEngine {
    private final HttpEngine mBackend;

    public AndroidHttpEngineWrapper(HttpEngine backend) {
        this.mBackend = backend;
    }

    @Override
    public String getVersionString() {
        return HttpEngine.getVersionString();
    }

    @Override
    public void shutdown() {
        mBackend.shutdown();
    }

    @Override
    public void startNetLogToFile(String fileName, boolean logAll) {
        // TODO(danstahr): Hidden API access
    }

    @Override
    public void stopNetLog() {
        // TODO(danstahr): Hidden API access
    }

    @Override
    public byte[] getGlobalMetricsDeltas() {
        // TODO(danstahr): Hidden API access
        return new byte[0];
    }

    @Override
    public URLConnection openConnection(URL url) throws IOException {
        return CronetExceptionTranslationUtils.executeTranslatingCronetExceptions(
                () -> mBackend.openConnection(url), IOException.class);
    }

    @Override
    public URLStreamHandlerFactory createURLStreamHandlerFactory() {
        return mBackend.createUrlStreamHandlerFactory();
    }

    @Override
    public org.chromium.net.ExperimentalBidirectionalStream.Builder newBidirectionalStreamBuilder(
            String url, org.chromium.net.BidirectionalStream.Callback callback, Executor executor) {
        return new AndroidBidirectionalStreamBuilderWrapper(mBackend.newBidirectionalStreamBuilder(
                url, executor, new BidirectionalStreamCallbackWrapper(callback)));
    }

    @Override
    public org.chromium.net.ExperimentalUrlRequest.Builder newUrlRequestBuilder(
            String url, org.chromium.net.UrlRequest.Callback callback, Executor executor) {
        return new AndroidUrlRequestBuilderWrapper(mBackend.newUrlRequestBuilder(
                url, executor, new UrlRequestCallbackWrapper(callback)));
    }
}
