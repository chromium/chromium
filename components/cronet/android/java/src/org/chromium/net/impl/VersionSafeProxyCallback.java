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
    private static final int SET_PROXY_OPTIONS_API_LEVEL = 38;

    static boolean apiContainsProxyOptions() {
        return VersionSafeCallbacks.ApiVersion.getMaximumAvailableApiLevel()
                >= SET_PROXY_OPTIONS_API_LEVEL;
    }

    private final @NonNull Proxy.Callback mBackend;

    VersionSafeProxyCallback(@NonNull Proxy.Callback backend) {
        if (!apiContainsProxyOptions()) {
            throw new AssertionError(
                    String.format(
                            "The Cronet APIs being used have an ApiLevel of %s, setProxyOptions was"
                                    + " added in ApiLevel %s. Since VersionSafeProxyCallback should"
                                    + " only be created when ProxyOptions are specified, this is"
                                    + " impossible!",
                            VersionSafeCallbacks.ApiVersion.getMaximumAvailableApiLevel(),
                            SET_PROXY_OPTIONS_API_LEVEL));
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
