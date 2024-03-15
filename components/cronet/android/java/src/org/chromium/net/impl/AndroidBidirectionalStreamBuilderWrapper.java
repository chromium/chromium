// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import android.net.Network;

import androidx.annotation.RequiresExtension;
import androidx.annotation.VisibleForTesting;

import org.chromium.net.CronetEngine;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidBidirectionalStreamBuilderWrapper
        extends org.chromium.net.ExperimentalBidirectionalStream.Builder {
    private final android.net.http.BidirectionalStream.Builder mBackend;
    private final AndroidBidirectionalStreamCallbackWrapper mWrappedCallback;

    public AndroidBidirectionalStreamBuilderWrapper(
            android.net.http.BidirectionalStream.Builder backend,
            AndroidBidirectionalStreamCallbackWrapper wrappedCallback) {
        mBackend = backend;
        mWrappedCallback = wrappedCallback;
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
    public org.chromium.net.ExperimentalBidirectionalStream.Builder bindToNetwork(
            long networkHandle) {
        // Network#fromNetworkHandle throws IAE if networkHandle does not translate to a valid
        // Network. Though, this can only happen if we're given a fake networkHandle (in which case
        // we will throw, which is fine).
        Network network =
                networkHandle == CronetEngine.UNBIND_NETWORK_HANDLE
                        ? null
                        : Network.fromNetworkHandle(networkHandle);
        // TODO(b/309112420): Stop no-op'ing this.
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
        return AndroidBidirectionalStreamWrapper.withRecordingToCallback(
                mBackend.build(), mWrappedCallback);
    }

    @VisibleForTesting
    AndroidBidirectionalStreamCallbackWrapper getCallback() {
        return mWrappedCallback;
    }
}
