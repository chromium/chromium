// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.NonNull;

import org.chromium.net.ProxyOptions;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

/** Wraps a {@link org.chromium.net.ProxyOptions} in a version safe manner. */
final class VersionSafeProxyOptions {
    private static final int SET_PROXY_OPTIONS_API_LEVEL = 38;

    private static boolean apiContainsProxyOptionsClass() {
        return VersionSafeCallbacks.ApiVersion.getMaximumAvailableApiLevel()
                >= SET_PROXY_OPTIONS_API_LEVEL;
    }

    private final @NonNull ProxyOptions mBackend;

    VersionSafeProxyOptions(@NonNull ProxyOptions backend) {
        if (!apiContainsProxyOptionsClass()) {
            throw new AssertionError(
                    String.format(
                            "This should have not been created: the Cronet API being used has an"
                                + " ApiLevel of %s, but setProxyOptions was added in ApiLevel %s",
                            VersionSafeCallbacks.ApiVersion.getMaximumAvailableApiLevel(),
                            SET_PROXY_OPTIONS_API_LEVEL));
        }
        mBackend = Objects.requireNonNull(backend);
        if (mBackend.getProxyList().isEmpty()) {
            throw new AssertionError(
                    "The list of proxies should never be empty, this is checked in the API layer");
        }
    }

    @NonNull
    List<VersionSafeProxyCallback> createProxyCallbackList() {
        // Note: the order in which proxies are added to this list must match the order of the
        // proxies returned by {@link createProxyOptionsProto}.
        List<VersionSafeProxyCallback> proxyCallbacks = new ArrayList<VersionSafeProxyCallback>();
        for (org.chromium.net.Proxy proxy : mBackend.getProxyList()) {
            // Fallback to a direct connection (also called fail-open or "direct proxy") is
            // represented by a null elemented in the proxy list (see
            // org.chromium.net.ProxyOptions).
            boolean isDirect = proxy == null;
            // ProxyDelegate callbacks should not be called for direct connection, hence we can
            // safely store a null callback in that case.
            proxyCallbacks.add(
                    isDirect
                            ? null
                            : new VersionSafeProxyCallback(
                                    proxy.getExecutor(), proxy.getCallback()));
        }
        return Collections.unmodifiableList(proxyCallbacks);
    }

    @NonNull
    org.chromium.net.impl.proto.ProxyOptions createProxyOptionsProto() {
        org.chromium.net.impl.proto.ProxyOptions.Builder proxyOptionsProtoBuilder =
                org.chromium.net.impl.proto.ProxyOptions.newBuilder();
        for (org.chromium.net.Proxy proxy : mBackend.getProxyList()) {
            org.chromium.net.impl.proto.Proxy.Builder proxyProtoBuilder =
                    org.chromium.net.impl.proto.Proxy.newBuilder();
            if (proxy == null) {
                proxyProtoBuilder.setScheme(org.chromium.net.impl.proto.ProxyScheme.DIRECT);
            } else {
                proxyProtoBuilder.setHost(proxy.getHost());
                proxyProtoBuilder.setPort(proxy.getPort());
                @org.chromium.net.Proxy.Scheme int scheme = proxy.getScheme();
                if (scheme == org.chromium.net.Proxy.SCHEME_HTTP) {
                    proxyProtoBuilder.setScheme(org.chromium.net.impl.proto.ProxyScheme.HTTP);
                } else if (scheme == org.chromium.net.Proxy.SCHEME_HTTPS) {
                    proxyProtoBuilder.setScheme(org.chromium.net.impl.proto.ProxyScheme.HTTPS);
                } else {
                    throw new AssertionError(
                            String.format(
                                    "Unknown Proxy.Scheme: %s. This should have been caught by the"
                                            + " API layer",
                                    scheme));
                }
            }
            // Note: the order in which proxies are added to this proto must match the order of the
            // callbacks returned by {@link createProxyCallbackList}.
            proxyOptionsProtoBuilder.addProxies(proxyProtoBuilder.build());
        }
        return proxyOptionsProtoBuilder.build();
    }
}
