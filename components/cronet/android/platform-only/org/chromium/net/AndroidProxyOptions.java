// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.net.http.HttpEngine;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.List;

final class AndroidProxyOptions {

    static void apply(
            @NonNull HttpEngine.Builder backend,
            @Nullable org.chromium.net.ProxyOptions proxyOptions) {
        if (proxyOptions == null) {
            backend.setProxyOptions(null);
            return;
        }

        List<android.net.http.Proxy> proxies = new ArrayList<android.net.http.Proxy>();
        for (org.chromium.net.Proxy proxy : proxyOptions.getProxyList()) {
            if (proxy != null) {
                proxies.add(AndroidProxy.fromCronetEngineProxy(proxy));
            } else {
                proxies.add(null);
            }
        }
        backend.setProxyOptions(android.net.http.ProxyOptions.fromProxyList(proxies));
    }

    private AndroidProxyOptions() {}
}
