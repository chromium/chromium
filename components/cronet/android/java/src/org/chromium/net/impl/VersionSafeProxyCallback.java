// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.net.Proxy;

import java.util.List;
import java.util.Map;
import java.util.Objects;

/** Wraps a {@link org.chromium.net.Proxy.Callback} in a version safe manner. */
final class VersionSafeProxyCallback {
    private static final int PROXY_CALLBACK_API_LEVEL = 38;

    private boolean apiContainsProxyCallbackClass() {
        return VersionSafeCallbacks.ApiVersion.getMaximumAvailableApiLevel()
                >= PROXY_CALLBACK_API_LEVEL;
    }

    private final @NonNull Proxy.Callback mBackend;

    VersionSafeProxyCallback(@NonNull Proxy.Callback backend) {
        if (!apiContainsProxyCallbackClass()) {
            throw new AssertionError(
                    String.format(
                            "This should not have been created: the Cronet API being used has an"
                                    + " ApiLevel of %s, but ProxyCallback was added in ApiLevel %s",
                            VersionSafeCallbacks.ApiVersion.getMaximumAvailableApiLevel(),
                            PROXY_CALLBACK_API_LEVEL));
        }
        mBackend = Objects.requireNonNull(backend);
    }

    @Nullable
    List<Map.Entry<String, String>> onBeforeTunnelRequest() {
        return mBackend.onBeforeTunnelRequest();
    }

    boolean onTunnelHeadersReceived(
            @NonNull List<Map.Entry<String, String>> responseHeaders, int statusCode) {
        return mBackend.onTunnelHeadersReceived(responseHeaders, statusCode);
    }
}
