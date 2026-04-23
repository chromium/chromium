// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static java.util.Objects.requireNonNull;

import android.content.Context;
import android.net.Network;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.net.ConnectivityManagerWrapper;
import org.chromium.net.httpflags.ResolvedFlags;

import java.net.URI;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.IdentityHashMap;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.atomic.AtomicReference;

/** Context and state management for {@link CronetAdaptiveNetworkBidirectionalStream}. */
class CronetAdaptiveRequestContext {
    // Name of the flag that controls which hosts are eligible for adaptive network selection.
    @VisibleForTesting
    public static final String ENABLE_ADAPTIVE_NETWORK_HOSTS_FLAG_NAME =
            "Cronet_enable_adaptive_network_hosts";

    // Name of the flag that controls which network paths are eligible for adaptive network
    // selection.
    @VisibleForTesting
    public static final String ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME =
            "Cronet_enable_adaptive_network_paths";

    // Name of the flag that controls how long we wait until we start the failover stream.
    @VisibleForTesting
    public static final String READY_FAILOVER_MS_FLAG_NAME = "Cronet_adaptive_failover_ms";

    // Name of the flag that controls whether Cronet adaptive network selection is enabled.
    public static final String ENABLE_ADAPTIVE_NETWORK_NAME = "Cronet_enable_adaptive_network";

    // Name of the flag that controls whether adaptive network is enabled for all hosts and paths.
    // FAST_IDEMPOTENT_PATHS_FLAG_NAME still needs to be configured for that behavior to trigger.
    @VisibleForTesting
    public static final String ENABLE_ADAPTIVE_NETWORK_FOR_ALL_NAME =
            "Cronet_enable_adaptive_network_for_all";

    // Name of the flag that controls which network paths are eligible for fast idempotent
    // selection. These paths must also be specified in the ENABLE_ADAPTIVE_NETWORK_PATHS_FLAG_NAME
    // flag, otherwise this will have no effect.
    @VisibleForTesting
    public static final String FAST_IDEMPOTENT_PATHS_FLAG_NAME = "Cronet_fast_idempotent_paths";

    // Name of the flag that controls whether a toast messages can appear when adaptive network
    // selection is used.
    // Only android developers should enable this flag, it is not intended to be used in production.
    @VisibleForTesting
    public static final String ADAPTIVE_NETWORK_DEV_TOAST_FLAG_NAME =
            "Cronet_adaptive_network_dev_toast";

    /**
     * The time we wait until we start the backup stream. This value is 3x the initial retransmit
     * timeout for TCP, we assume that a connection with reasonable performance will be open within
     * this timeframe.
     */
    private static final long DEFAULT_READY_FAILOVER_MS = 3000;

    /** Default value for how long to remember a fallback network for a given host. */
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

    private final Context mContext;
    private final Clock mClock;
    private final CronetLogger mLogger;
    private final String[] mAdaptiveNetworkHosts;
    private final Set<String> mAdaptiveNetworkPaths;
    private final Set<String> mFastIdempotentPaths;
    private final long mReadyFailoverMs;
    private final boolean mEnableAdaptiveNetwork;
    private final boolean mEnableAdaptiveNetworkForAll;
    @Nullable private final Handler mToastHandler;

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

    /** Container for network handles to be used for a bidirectional stream. */
    static class AdaptiveStreamNetworkHandles {
        final long mPrimaryNetworkHandle;
        final long mFallbackNetworkHandle;

        AdaptiveStreamNetworkHandles(long primaryNetworkHandle, long fallbackNetworkHandle) {
            mPrimaryNetworkHandle = primaryNetworkHandle;
            mFallbackNetworkHandle = fallbackNetworkHandle;
        }
    }

    // Keep a basic mapping of host to fallback network info, we don't expect a large number of
    // hosts.
    private final Map<String, FallbackInfo> mFallbackNetworks =
            Collections.synchronizedMap(new HashMap<>());

    // Keeps track of the adaptive streams that have started but do not yet have a winner.
    private final Set<CronetAdaptiveNetworkBidirectionalStream> mActiveStreams =
            Collections.synchronizedSet(Collections.newSetFromMap(new IdentityHashMap<>()));

    private final AtomicReference<ScheduledExecutorService> mExecutor = new AtomicReference<>(null);
    private ConnectivityManagerWrapper mConnectivityManagerWrapper;

    public CronetAdaptiveRequestContext(Context context, CronetLogger logger) {
        this(context, logger, new DefaultClock());
    }

    @VisibleForTesting
    CronetAdaptiveRequestContext(Context context, CronetLogger logger, Clock clock) {
        mContext = context.getApplicationContext();
        mLogger = logger;
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

        if (flags.containsKey(ENABLE_ADAPTIVE_NETWORK_NAME)) {
            mEnableAdaptiveNetwork = flags.get(ENABLE_ADAPTIVE_NETWORK_NAME).getBoolValue();
        } else {
            mEnableAdaptiveNetwork = false;
        }

        if (flags.containsKey(ENABLE_ADAPTIVE_NETWORK_FOR_ALL_NAME)) {
            mEnableAdaptiveNetworkForAll =
                    flags.get(ENABLE_ADAPTIVE_NETWORK_FOR_ALL_NAME).getBoolValue();
        } else {
            mEnableAdaptiveNetworkForAll = false;
        }

        if (flags.containsKey(ADAPTIVE_NETWORK_DEV_TOAST_FLAG_NAME)
                && flags.get(ADAPTIVE_NETWORK_DEV_TOAST_FLAG_NAME).getBoolValue()) {
            mToastHandler = new Handler(Looper.getMainLooper());
        } else {
            mToastHandler = null;
        }

        mFastIdempotentPaths = new HashSet<>();
        if (flags.containsKey(FAST_IDEMPOTENT_PATHS_FLAG_NAME)) {
            for (String path :
                    flags.get(FAST_IDEMPOTENT_PATHS_FLAG_NAME).getStringValue().trim().split(",")) {
                if (!path.isEmpty()) {
                    mFastIdempotentPaths.add(path);
                }
            }
        }
    }

    /**
     * Returns AdaptiveStreamNetworkHandles to use given a URL.
     *
     * @param url The URL to compute the network handles for.
     * @param streamDefaultNetworkHandle the network we would default to.
     * @return null if no special network handling should be used.
     */
    @Nullable
    public AdaptiveStreamNetworkHandles computeStreamNetworkHandles(
            URI uri, long streamDefaultNetworkHandle) {
        if (!mEnableAdaptiveNetwork) {
            return null;
        }

        try (var traceEvent =
                ScopedSysTraceEvent.scoped(
                        "CronetAdaptiveRequestContext#computeStreamNetworkHandles")) {
            // Get available networks.
            Network[] networks = getAllNetworks();
            Network defaultNetwork = mConnectivityManagerWrapper.getDefaultNetwork();

            // If we have data, from previous streams, that indicates what network works for
            // this host, use that.
            Long memorizedFallback = getMemorizedFallbackNetworkHandle(uri, networks);
            boolean useMemorizedFallback =
                    memorizedFallback != null && memorizedFallback != streamDefaultNetworkHandle;
            mLogger.logCronetAdaptiveTrafficAlternateNetworkComputation(
                    NativeCronetEngineBuilderImpl.getCronetSource(),
                    networks.length,
                    defaultNetwork != null,
                    useMemorizedFallback);
            if (useMemorizedFallback) {
                return new AdaptiveStreamNetworkHandles(
                        memorizedFallback, streamDefaultNetworkHandle);
            }

            // Otherwise, try to find a fallback network from scratch.
            Long fallbackNetworkHandle = computeAlternativeNetworkHandle(networks, defaultNetwork);
            if (fallbackNetworkHandle != null
                    && fallbackNetworkHandle == streamDefaultNetworkHandle) {
                fallbackNetworkHandle = null;
            }

            if (fallbackNetworkHandle != null) {
                return new AdaptiveStreamNetworkHandles(
                        streamDefaultNetworkHandle, fallbackNetworkHandle);
            }

            return null;
        }
    }

    /** Returns true if the given URI is configured as a fast idempotent request. */
    public boolean isFastIdempotentRequest(URI uri) {
        return mFastIdempotentPaths.contains(uri.getPath());
    }

    /** Registers a stream as active without a winner yet. */
    void registerStream(CronetAdaptiveNetworkBidirectionalStream stream) {
        if (!mActiveStreams.add(stream)) {
            throw new AssertionError("Stream is already registered as active.");
        }
    }

    /** Unregisters a stream that has either completed or has a winner. */
    void unregisterStream(CronetAdaptiveNetworkBidirectionalStream stream) {
        if (!mActiveStreams.remove(stream)) {
            throw new AssertionError("Stream is not registered as active.");
        }
    }

    long getReadyFailoverMs() {
        return mReadyFailoverMs;
    }

    /** Reports that the fallback network was used for the given URL. */
    void reportFallbackUsed(String url, Long networkHandle) {
        URI parsedUri = URI.create(url);
        String host = parsedUri.getHost();

        maybeShowDevToast(
                host,
                parsedUri.getPath(),
                /* isDefault= */ networkHandle == CronetEngineBase.DEFAULT_NETWORK_HANDLE);

        tellOtherStreamsAboutFallback(parsedUri, networkHandle);

        // If we started succeeding on the default network, we can reset the state for this host.
        if (networkHandle == CronetEngineBase.DEFAULT_NETWORK_HANDLE) {
            mFallbackNetworks.remove(host);
            return;
        }
        mFallbackNetworks.put(
                host,
                new FallbackInfo(
                        networkHandle,
                        mClock.elapsedRealtime() + DEFAULT_FALLBACK_CACHE_DURATION_MS));
    }

    private void tellOtherStreamsAboutFallback(URI parsedUri, Long networkHandle) {
        synchronized (mActiveStreams) {
            var iterator = mActiveStreams.iterator();
            while (iterator.hasNext()) {
                CronetAdaptiveNetworkBidirectionalStream stream = iterator.next();
                stream.reportOtherStreamFallback(parsedUri, networkHandle);
            }
        }
    }

    @VisibleForTesting
    void maybeShowDevToast(String host, String path, boolean isDefault) {
        if (mToastHandler != null) {
            mToastHandler.post(
                    () ->
                            Toast.makeText(
                                            mContext,
                                            "CRONET DEV: Fallback network used "
                                                    + host
                                                    + path
                                                    + " is default: "
                                                    + isDefault,
                                            Toast.LENGTH_LONG)
                                    .show());
        }
    }

    /**
     * Returns the network handle to use for the given URL if a fallback was recently used for its
     * host, or {@code null} if no fallback is cached.
     */
    private Long getMemorizedFallbackNetworkHandle(URI parsedUri, Network[] networks) {
        String host = parsedUri.getHost();
        FallbackInfo info = mFallbackNetworks.get(host);
        if (info != null) {
            if (mClock.elapsedRealtime() < info.mExpirationTimeMs) {
                // Double check that the network is still available to us.
                for (Network network : networks) {
                    if (network.getNetworkHandle() == info.mNetworkHandle) {
                        return info.mNetworkHandle;
                    }
                }
            }
            mFallbackNetworks.remove(host);
        }
        return null;
    }

    /**
     * Returns the parsed URI if the given URL is configured as an adaptive network URL, or null
     * otherwise.
     */
    @Nullable
    URI getUriIfAdaptive(String url) {
        try (var traceEvent =
                ScopedSysTraceEvent.scoped("CronetAdaptiveRequestContext#getUriIfAdaptive")) {
            if (mEnableAdaptiveNetworkForAll) {
                return URI.create(url);
            }
            for (String host : mAdaptiveNetworkHosts) {
                if (!host.isEmpty() && url.startsWith(host)) {
                    URI parsedUri = URI.create(url);
                    if (mAdaptiveNetworkPaths.contains(parsedUri.getPath())) {
                        return parsedUri;
                    }
                }
            }
            return null;
        }
    }

    ScheduledExecutorService getOrCreateScheduledExecutor() {
        if (mExecutor.get() == null) {
            mExecutor.compareAndSet(null, Executors.newSingleThreadScheduledExecutor());
        }
        return mExecutor.get();
    }

    /** Returns all available networks. */
    @VisibleForTesting
    Network[] getAllNetworks() {
        // ConnectivityManagerWrapper#getAllNetworks contains logic not to bypass a VPN if one is
        // set (it will always return only the VPN network in that case). This logic is bypassed
        // if the VPN is being ignored via the ignoreNetwork parameter. With that in mind, do not
        // ignore the default network within ConnectivityManagerWrapper#getAllNetworks, but do it
        // in a post-processing step, in the scenario that the default network is a VPN.
        return mConnectivityManagerWrapper.getAllNetworks(/* ignoreNetwork= */ null);
    }

    /** Returns an alternative network handle, or {@code null} if none is available. */
    @VisibleForTesting
    Long computeAlternativeNetworkHandle(Network[] networks, Network defaultNetwork) {
        for (Network network : networks) {
            if (network != null && !network.equals(defaultNetwork)) {
                return network.getNetworkHandle();
            }
        }
        return null;
    }

    void setConnectivityManagerWrapperForTest(
            ConnectivityManagerWrapper connectivityManagerWrapper) {
        mConnectivityManagerWrapper = connectivityManagerWrapper;
    }
}
