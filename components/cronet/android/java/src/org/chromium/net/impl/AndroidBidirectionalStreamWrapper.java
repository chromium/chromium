// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import androidx.annotation.RequiresExtension;

import java.nio.ByteBuffer;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidBidirectionalStreamWrapper extends org.chromium.net.ExperimentalBidirectionalStream {
    private final android.net.http.BidirectionalStream mBackend;

    private AndroidBidirectionalStreamWrapper(android.net.http.BidirectionalStream backend) {
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

    /**
     * Creates an {@link AndroidBidirectionalStreamWrapper} that is stored on the callback.
     *
     * @param backend the http BidirectionalStream
     * @param callback the stream's callback
     * @return
     */
    static AndroidBidirectionalStreamWrapper withRecordingToCallback(
            android.net.http.BidirectionalStream backend,
            AndroidBidirectionalStreamCallbackWrapper callback) {
        AndroidBidirectionalStreamWrapper wrappedStream =
                new AndroidBidirectionalStreamWrapper(backend);
        callback.recordWrappedStream(wrappedStream);
        return wrappedStream;
    }

    android.net.http.BidirectionalStream getBackend() {
        return mBackend;
    }
}
