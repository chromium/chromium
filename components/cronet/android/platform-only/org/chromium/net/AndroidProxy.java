// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import androidx.annotation.NonNull;

import java.util.Objects;

final class AndroidProxy {

    @NonNull
    static android.net.http.Proxy fromCronetEngineProxy(@NonNull org.chromium.net.Proxy proxy) {
        Objects.requireNonNull(proxy);
        return android.net.http.Proxy.createHttpProxy(
                translateScheme(proxy.getScheme()),
                proxy.getHost(),
                proxy.getPort(),
                proxy.getExecutor(),
                new AndroidProxyHttpConnectCallback(proxy.getCallback()));
    }

    private static int translateScheme(@org.chromium.net.Proxy.Scheme int scheme) {
        return switch (scheme) {
            case android.net.http.Proxy.SCHEME_HTTP -> android.net.http.Proxy.SCHEME_HTTP;
            case android.net.http.Proxy.SCHEME_HTTPS -> android.net.http.Proxy.SCHEME_HTTPS;
            default -> throw new AssertionError(String.format("Unknown scheme %d", scheme));
        };
    }

    private AndroidProxy() {}
}
