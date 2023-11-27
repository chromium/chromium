// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import android.net.Network;
import android.net.http.HttpEngine;

import androidx.annotation.RequiresExtension;

import org.chromium.net.ExperimentalCronetEngine;

import java.io.IOException;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLStreamHandlerFactory;
import java.util.concurrent.Executor;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
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
    public void bindToNetwork(long networkHandle) {
        // Network#fromNetworkHandle throws IAE if networkHandle does not translate to a valid
        // Network. Though, this can only happen if we're given a fake networkHandle (in which case
        // we will throw, which is fine).
        Network network =
                networkHandle == UNBIND_NETWORK_HANDLE
                        ? null
                        : Network.fromNetworkHandle(networkHandle);
        mBackend.bindToNetwork(network);
    }

    @Override
    public URLConnection openConnection(URL url) throws IOException {
        return CronetExceptionTranslationUtils.executeTranslatingCronetExceptions(
                () -> mBackend.openConnection(url), IOException.class);
    }

    @Override
    public URLConnection openConnection(URL url, Proxy proxy) throws IOException {
        // HttpEngine doesn't expose an openConnection(URL, Proxy) method. To maintain compatibility
        // copy-paste CronetUrlRequestContext's logic here.
        if (proxy.type() != Proxy.Type.DIRECT) {
            throw new UnsupportedOperationException();
        }
        String protocol = url.getProtocol();
        if ("http".equals(protocol) || "https".equals(protocol)) {
            return openConnection(url);
        }
        throw new UnsupportedOperationException("Unexpected protocol:" + protocol);
    }

    @Override
    public URLStreamHandlerFactory createURLStreamHandlerFactory() {
        return mBackend.createUrlStreamHandlerFactory();
    }

    @Override
    public org.chromium.net.ExperimentalBidirectionalStream.Builder newBidirectionalStreamBuilder(
            String url, org.chromium.net.BidirectionalStream.Callback callback, Executor executor) {
        return new AndroidBidirectionalStreamBuilderWrapper(
                mBackend.newBidirectionalStreamBuilder(
                        url, executor, new AndroidBidirectionalStreamCallbackWrapper(callback)));
    }

    @Override
    public org.chromium.net.ExperimentalUrlRequest.Builder newUrlRequestBuilder(
            String url, org.chromium.net.UrlRequest.Callback callback, Executor executor) {
        return new AndroidUrlRequestBuilderWrapper(
                mBackend.newUrlRequestBuilder(
                        url, executor, new AndroidUrlRequestCallbackWrapper(callback)));
    }
}
