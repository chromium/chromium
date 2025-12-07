// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.util.Pair;

import androidx.annotation.NonNull;

import java.util.List;

final class AndroidProxyHttpConnectCallback implements android.net.http.Proxy.HttpConnectCallback {

    @Override
    public void onBeforeRequest(
            @NonNull android.net.http.Proxy.HttpConnectCallback.Request request) {
        mBackend.onBeforeRequest(new AndroidProxyHttpConnectCallbackRequest(request));
    }

    @Override
    public int onResponseReceived(
            @NonNull List<Pair<String, String>> responseHeaders, int statusCode) {
        @org.chromium.net.Proxy.HttpConnectCallback.OnResponseReceivedAction
        int result = mBackend.onResponseReceived(responseHeaders, statusCode);
        return switch (result) {
            case org.chromium.net.Proxy.HttpConnectCallback.RESPONSE_ACTION_CLOSE ->
                    android.net.http.Proxy.HttpConnectCallback.RESPONSE_ACTION_CLOSE;
            case org.chromium.net.Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED ->
                    android.net.http.Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED;
            default ->
                    throw new AssertionError(
                            String.format("Unknown OnResponseReceivedAction: %d", result));
        };
    }

    AndroidProxyHttpConnectCallback(org.chromium.net.Proxy.HttpConnectCallback backend) {
        mBackend = backend;
    }

    private final org.chromium.net.Proxy.HttpConnectCallback mBackend;
}
