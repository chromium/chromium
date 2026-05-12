// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.net.http.HttpEngine;
import android.os.Build;
import android.os.ext.SdkExtensions;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.List;

public final class AndroidProxyOptions {

    public static void apply(
            @NonNull HttpEngine.Builder backend,
            @Nullable org.chromium.net.ProxyOptions proxyOptions) {
        if (!(Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                && SdkExtensions.getExtensionVersion(Build.VERSION_CODES.S) >= 22)) {
            throw new UnsupportedOperationException(
                    "This Cronet implementation does not support ProxyOptions");
        }

        if (proxyOptions == null) {
            backend.setProxyOptions(null);
            return;
        }

        List<android.net.http.Proxy> proxies = new ArrayList<>();
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
        backend.setProxyOptions(
                android.net.http.ProxyOptions.fromProxyList(proxies, allProxiesFailedBehavior));
    }

    private AndroidProxyOptions() {}
}
