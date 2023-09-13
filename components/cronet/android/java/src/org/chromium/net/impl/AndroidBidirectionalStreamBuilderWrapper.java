// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.RequiresApi;

@RequiresApi(api = 34)
class AndroidBidirectionalStreamBuilderWrapper
        extends org.chromium.net.ExperimentalBidirectionalStream.Builder {
    private final android.net.http.BidirectionalStream.Builder mBackend;

    public AndroidBidirectionalStreamBuilderWrapper(
            android.net.http.BidirectionalStream.Builder backend) {
        this.mBackend = backend;
    }

    @Override
    public org.chromium.net.ExperimentalBidirectionalStream.Builder setHttpMethod(String method) {
        mBackend.setHttpMethod(method);
        return this;
    }

    @Override
    public org.chromium.net.ExperimentalBidirectionalStream.Builder addHeader(
            String header, String value) {
        mBackend.addHeader(header, value);
        return this;
    }

    @Override
    public org.chromium.net.ExperimentalBidirectionalStream.Builder setPriority(int priority) {
        mBackend.setPriority(priority);
        return this;
    }

    @Override
    public org.chromium.net.ExperimentalBidirectionalStream.Builder
    delayRequestHeadersUntilFirstFlush(boolean delayRequestHeadersUntilFirstFlush) {
        mBackend.setDelayRequestHeadersUntilFirstFlushEnabled(delayRequestHeadersUntilFirstFlush);
        return this;
    }

    @Override
    public org.chromium.net.ExperimentalBidirectionalStream build() {
        return new AndroidBidirectionalStreamWrapper(mBackend.build());
    }
}
