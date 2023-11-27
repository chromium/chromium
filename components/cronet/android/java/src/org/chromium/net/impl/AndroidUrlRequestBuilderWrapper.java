// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import android.net.Network;

import androidx.annotation.RequiresExtension;

import org.chromium.net.CronetEngine;

import java.util.concurrent.Executor;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidUrlRequestBuilderWrapper extends org.chromium.net.ExperimentalUrlRequest.Builder {
    private final android.net.http.UrlRequest.Builder mBackend;

    public AndroidUrlRequestBuilderWrapper(android.net.http.UrlRequest.Builder backend) {
        this.mBackend = backend;
    }

    @Override
    public org.chromium.net.ExperimentalUrlRequest.Builder setHttpMethod(String method) {
        mBackend.setHttpMethod(method);
        return this;
    }

    @Override
    public org.chromium.net.ExperimentalUrlRequest.Builder addHeader(String header, String value) {
        mBackend.addHeader(header, value);
        return this;
    }

    @Override
    public org.chromium.net.ExperimentalUrlRequest.Builder disableCache() {
        mBackend.setCacheDisabled(true);
        return this;
    }

    @Override
    public org.chromium.net.ExperimentalUrlRequest.Builder setPriority(int priority) {
        mBackend.setPriority(priority);
        return this;
    }

    @Override
    public org.chromium.net.ExperimentalUrlRequest.Builder setUploadDataProvider(
            org.chromium.net.UploadDataProvider uploadDataProvider, Executor executor) {
        mBackend.setUploadDataProvider(
                new AndroidUploadDataProviderWrapper(uploadDataProvider), executor);
        return this;
    }

    @Override
    public org.chromium.net.ExperimentalUrlRequest.Builder allowDirectExecutor() {
        mBackend.setDirectExecutorAllowed(true);
        return this;
    }

    @Override
    public org.chromium.net.ExperimentalUrlRequest.Builder bindToNetwork(long networkHandle) {
        // Network#fromNetworkHandle throws IAE if networkHandle does not translate to a valid
        // Network. Though, this can only happen if we're given a fake networkHandle (in which case
        // we will throw, which is fine).
        Network network =
                networkHandle == CronetEngine.UNBIND_NETWORK_HANDLE
                        ? null
                        : Network.fromNetworkHandle(networkHandle);
        mBackend.bindToNetwork(network);
        return this;
    }

    @Override
    public org.chromium.net.ExperimentalUrlRequest build() {
        return new AndroidUrlRequestWrapper(mBackend.build());
    }
}
