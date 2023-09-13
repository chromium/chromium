// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.RequiresApi;

import java.util.concurrent.Executor;

@RequiresApi(api = 34)
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
        mBackend.setUploadDataProvider(new UploadDataProviderWrapper(uploadDataProvider), executor);
        return this;
    }

    @Override
    public org.chromium.net.ExperimentalUrlRequest.Builder allowDirectExecutor() {
        mBackend.setDirectExecutorAllowed(true);
        return this;
    }

    @Override
    public org.chromium.net.ExperimentalUrlRequest build() {
        return new AndroidUrlRequestWrapper(mBackend.build());
    }
}
