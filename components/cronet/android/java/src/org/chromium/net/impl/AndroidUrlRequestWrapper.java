// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.RequiresApi;

import java.nio.ByteBuffer;

@RequiresApi(34)
class AndroidUrlRequestWrapper extends org.chromium.net.ExperimentalUrlRequest {
    private final android.net.http.UrlRequest mBackend;

    AndroidUrlRequestWrapper(android.net.http.UrlRequest backend) {
        this.mBackend = backend;
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
        mBackend.getStatus(new UrlRequestStatusListenerWrapper(listener));
    }
}
