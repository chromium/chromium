// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.util.Pair;

import androidx.annotation.NonNull;

import java.util.List;

final class AndroidProxyHttpConnectCallbackRequest
        extends org.chromium.net.Proxy.HttpConnectCallback.Request {

    @Override
    public void proceed(@NonNull List<Pair<String, String>> extraHeaders) {
        mBackend.proceed(extraHeaders);
    }

    @Override
    public void close() {
        mBackend.close();
    }

    AndroidProxyHttpConnectCallbackRequest(
            @NonNull android.net.http.Proxy.HttpConnectCallback.Request backend) {
        mBackend = backend;
    }

    private final android.net.http.Proxy.HttpConnectCallback.Request mBackend;
}
