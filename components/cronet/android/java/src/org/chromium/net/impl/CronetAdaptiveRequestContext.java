// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static java.util.Objects.requireNonNull;

import android.content.Context;
import android.net.Network;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.net.ConnectivityManagerWrapper;
import org.chromium.net.httpflags.ResolvedFlags;

import java.net.URI;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.atomic.AtomicReference;

/** Context and state management for {@link CronetAdaptiveNetworkBidirectionalStream}. */
class CronetAdaptiveRequestContext {
    @VisibleForTesting
    public static final String ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME =
            "Cronet_enable_adaptive_network_hosts";

    @VisibleForTesting
    public static final String ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME =
            "Cronet_enable_adaptive_network_paths";

    @VisibleForTesting
    public static final String READY_FAILOVER_MS_FLAG_NAME = "Cronet_adaptive_failover_ms";

    /**
     * The time we wait until we start the backup stream. This value is 3x the initial retransmit
     * timeout for TCP, we assume that a connection with reasonable performance will be open within
     * this timeframe.
     */
    private static final long DEFAULT_READY_FAILOVER_MS = 3000;

    /** Default value how long to remember a fallback network for a given host. */
    private static final long DEFAULT_FALLBACK_CACHE_DURATION_MS = 10000;

    @VisibleForTesting
    abstract static class Clock {
        abstract long elapsedRealtime();
    }

    private static class DefaultClock extends Clock {
        @Override
        long elapsedRealtime() {
            return SystemClock.elapsedRealtime();
        }
    }

    private final Clock mClock;
    private final String[] mAdaptiveNetworkHosts;
    private final Set<String> mAdaptiveNetworkPaths;
    private final long mReadyFailoverMs;

    /** Information about a fallback network for a given host. */
    private static class FallbackInfo {
        // The network handle that was successfully used for a connection.
        final Long mNetworkHandle;
        // The time when the fallback network information expires.
        final long mExpirationTimeMs;

        FallbackInfo(Long networkHandle, long expirationTimeMs) {
            mNetworkHandle = requireNonNull(networkHandle);
            mExpirationTimeMs = expirationTimeMs;
        }
    }

    // Keep a basic mapping of host to fallback network info, we don't expect a large number of
    // hosts.
    private final Map<String, FallbackInfo> mFallbackNetworks =
            Collections.synchronizedMap(new HashMap<>());

    private final AtomicReference<ScheduledExecutorService> mExecutor = new AtomicReference<>(null);
    private ConnectivityManagerWrapper mConnectivityManagerWrapper;

    public CronetAdaptiveRequestContext(Context context) {
        this(context, new DefaultClock());
    }

    @VisibleForTesting
    CronetAdaptiveRequestContext(Context context, Clock clock) {
        mClock = clock;
        mConnectivityManagerWrapper = new ConnectivityManagerWrapper(context);
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

        if (flags.containsKey(READY_FAILOVER_MS_FLAG_NAME)) {
            mReadyFailoverMs = flags.get(READY_FAILOVER_MS_FLAG_NAME).getIntValue();
        } else {
            mReadyFailoverMs = DEFAULT_READY_FAILOVER_MS;
        }
    }

    long getReadyFailoverMs() {
        return mReadyFailoverMs;
    }

    /** Reports that the fallback network was used for the given URL. */
    void reportFallbackUsed(String url, Long networkHandle) {
        if (networkHandle == CronetEngineBase.DEFAULT_NETWORK_HANDLE) {
            throw new IllegalArgumentException("Network handle must be non-default.");
        }
        URI parsedUri = URI.create(url);
        String host = parsedUri.getHost();
        mFallbackNetworks.put(
                host,
                new FallbackInfo(
                        networkHandle,
                        mClock.elapsedRealtime() + DEFAULT_FALLBACK_CACHE_DURATION_MS));
    }

    /**
     * Returns the network handle to use for the given URL if a fallback was recently used for its
     * host, or {@code null} if no fallback is cached.
     */
    Long getFallbackNetworkHandle(String url) {
        URI parsedUri = URI.create(url);
        String host = parsedUri.getHost();
        FallbackInfo info = mFallbackNetworks.get(host);
        if (info != null) {
            if (mClock.elapsedRealtime() < info.mExpirationTimeMs) {
                return info.mNetworkHandle;
            }
            mFallbackNetworks.remove(host);
        }
        return null;
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

    /** Returns an alternative network handle, or {@code null} if none is available. */
    public Long computeAlternativeNetworkHandle() {
        Network[] networks =
                mConnectivityManagerWrapper.getAllNetworks(
                        mConnectivityManagerWrapper.getDefaultNetwork());
        if (networks.length > 0) {
            return networks[0].getNetworkHandle();
        }
        return null;
    }

    void setConnectivityManagerWrapperForTest(
            ConnectivityManagerWrapper connectivityManagerWrapper) {
        mConnectivityManagerWrapper = connectivityManagerWrapper;
    }
}
