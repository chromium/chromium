// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.RequiresApi;

import java.nio.ByteBuffer;

@RequiresApi(api = 34)
class AndroidBidirectionalStreamWrapper extends org.chromium.net.ExperimentalBidirectionalStream {
    private final android.net.http.BidirectionalStream mBackend;

    AndroidBidirectionalStreamWrapper(android.net.http.BidirectionalStream backend) {
        this.mBackend = backend;
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
}
