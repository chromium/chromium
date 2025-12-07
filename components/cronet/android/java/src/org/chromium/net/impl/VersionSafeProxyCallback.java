// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.util.Pair;

import androidx.annotation.NonNull;

import org.chromium.net.Proxy;

import java.util.List;
import java.util.Objects;
import java.util.concurrent.Executor;

/** Wraps a {@link org.chromium.net.Proxy.HttpConnectCallback} in a version safe manner. */
final class VersionSafeProxyCallback {
    private static final int PROXY_CALLBACK_API_LEVEL = 38;

    private boolean apiContainsProxyCallbackClass() {
        return VersionSafeCallbacks.ApiVersion.getMaximumAvailableApiLevel()
                >= PROXY_CALLBACK_API_LEVEL;
    }

    private final @NonNull Proxy.HttpConnectCallback mBackend;
    private final @NonNull Executor mExecutor;

    VersionSafeProxyCallback(
            @NonNull Executor executor, @NonNull Proxy.HttpConnectCallback backend) {
        if (!apiContainsProxyCallbackClass()) {
            throw new AssertionError(
                    String.format(
                            "This should not have been created: the Cronet API being used has an"
                                    + " ApiLevel of %s, but ProxyCallback was added in ApiLevel %s",
                            VersionSafeCallbacks.ApiVersion.getMaximumAvailableApiLevel(),
                            PROXY_CALLBACK_API_LEVEL));
        }
        mExecutor = Objects.requireNonNull(executor);
        mBackend = Objects.requireNonNull(backend);
    }

    @NonNull
    Executor getExecutor() {
        return mExecutor;
    }

    void onBeforeTunnelRequest(Proxy.HttpConnectCallback.Request request) {
        mBackend.onBeforeRequest(request);
    }

    boolean onTunnelHeadersReceived(
            @NonNull List<Pair<String, String>> responseHeaders, int statusCode) {
        return mBackend.onResponseReceived(responseHeaders, statusCode)
                == Proxy.HttpConnectCallback.RESPONSE_ACTION_PROCEED;
    }
}
