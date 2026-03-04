// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.net.httpflags.ResolvedFlags;

import java.net.URI;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.atomic.AtomicReference;

/** Context and state management for {@link CronetAdaptiveNetworkBidirectionalStream.java}. */
final class CronetAdaptiveRequestContext {
    @VisibleForTesting
    public static final String ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME =
            "Cronet_enable_adaptive_network_hosts";

    @VisibleForTesting
    public static final String ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME =
            "Cronet_enable_adaptive_network_paths";

    private final String[] mAdaptiveNetworkHosts;
    private final Set<String> mAdaptiveNetworkPaths;
    private final AtomicReference<ScheduledExecutorService> mExecutor = new AtomicReference<>(null);

    public CronetAdaptiveRequestContext(Context context) {
        Map<String, ResolvedFlags.Value> flags =
                HttpFlagsForImpl.getHttpFlags(
                                context, NativeCronetEngineBuilderImpl.getCronetSource())
                        .flags();

        if (flags.containsKey(ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME)) {
            mAdaptiveNetworkHosts =
                    flags.get(ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME)
                            .getStringValue()
                            .trim()
                            .split(",");
        } else {
            mAdaptiveNetworkHosts = new String[0];
        }

        mAdaptiveNetworkPaths = new HashSet<>();
        if (flags.containsKey(ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME)) {
            for (String path :
                    flags.get(ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME)
                            .getStringValue()
                            .trim()
                            .split(",")) {
                if (!path.isEmpty()) {
                    mAdaptiveNetworkPaths.add(path);
                }
            }
        }
    }

    ScheduledExecutorService getOrCreateScheduledExecutor() {
        if (mExecutor.get() == null) {
            mExecutor.compareAndSet(null, Executors.newSingleThreadScheduledExecutor());
        }
        return mExecutor.get();
    }

    /** Returns true if the given URL is configured as an adaptive network URL. */
    boolean isAdaptiveNetworkUrl(String url) {
        try (var traceEvent =
                ScopedSysTraceEvent.scoped("CronetAdaptiveRequestContext#isAdaptiveNetworkUrl")) {
            for (String host : mAdaptiveNetworkHosts) {
                if (url.startsWith(host)) {
                    URI parsedUri = URI.create(url);
                    return mAdaptiveNetworkPaths.contains(parsedUri.getPath());
                }
            }
            return false;
        }
    }
}
