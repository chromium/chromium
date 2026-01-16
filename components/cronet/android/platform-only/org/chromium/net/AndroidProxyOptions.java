// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.net.http.HttpEngine;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.List;

public final class AndroidProxyOptions {

    public static void apply(
            @NonNull HttpEngine.Builder backend,
            @Nullable org.chromium.net.ProxyOptions proxyOptions) {
        if (proxyOptions == null) {
            backend.setProxyOptions(null);
            return;
        }

        List<android.net.http.Proxy> proxies = new ArrayList<android.net.http.Proxy>();
        int allProxiesFailedBehavior =
                android.net.http.ProxyOptions.ALL_PROXIES_FAILED_BEHAVIOR_DISALLOW_DIRECT;
        for (org.chromium.net.Proxy proxy : proxyOptions.getProxyList()) {
            if (proxy != null) {
                proxies.add(AndroidProxy.fromCronetEngineProxy(proxy));
            } else {
                allProxiesFailedBehavior =
                        android.net.http.ProxyOptions.ALL_PROXIES_FAILED_BEHAVIOR_ALLOW_DIRECT;
            }
        }
        if (proxies.isEmpty()) {
            // CronetEngine accepts a list of proxies containing only the fallback option.
            // HttpEngine does not. Until the two converge, translate this to a no-op on the
            // underlying HttpEngine.
            return;
        }
        backend.setProxyOptions(
                android.net.http.ProxyOptions.fromProxyList(proxies, allProxiesFailedBehavior));
    }

    private AndroidProxyOptions() {}
}