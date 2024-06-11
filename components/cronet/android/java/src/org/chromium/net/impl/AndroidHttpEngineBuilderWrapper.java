// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_API_LEVEL;
import static org.chromium.net.impl.HttpEngineNativeProvider.EXT_VERSION;

import android.net.http.HttpEngine;
import android.os.Process;
import android.util.Log;

import androidx.annotation.RequiresExtension;
import androidx.annotation.VisibleForTesting;

import org.chromium.net.CronetEngine;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.ICronetEngineBuilder;
import org.chromium.net.telemetry.ExperimentalOptions;
import org.chromium.net.telemetry.OptionalBoolean;

import java.time.Duration;
import java.util.Date;
import java.util.Set;

@RequiresExtension(extension = EXT_API_LEVEL, version = EXT_VERSION)
class AndroidHttpEngineBuilderWrapper extends ICronetEngineBuilder {
    private static final String TAG = "HttpEngBuilderWrap";

    private static boolean sLibraryLoaderUnsupportedLogged;
    private static boolean sNQEUnsupportedLogged;

    private final HttpEngine.Builder mBackend;
    private int mThreadPriority = Integer.MIN_VALUE;

    public AndroidHttpEngineBuilderWrapper(HttpEngine.Builder backend) {
        this.mBackend = backend;
    }

    @Override
    public String getDefaultUserAgent() {
        return mBackend.getDefaultUserAgent();
    }

    @Override
    public ICronetEngineBuilder setUserAgent(String userAgent) {
        mBackend.setUserAgent(userAgent);
        return this;
    }

    @Override
    public ICronetEngineBuilder setStoragePath(String value) {
        mBackend.setStoragePath(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder setLibraryLoader(CronetEngine.Builder.LibraryLoader loader) {
        if (!sLibraryLoaderUnsupportedLogged) {
            Log.i(
                    TAG,
                    "Custom library loader is unsupported when HttpEngineNativeProvider is used.");
            sLibraryLoaderUnsupportedLogged = true;
        }
        return this;
    }

    @Override
    public ICronetEngineBuilder enableQuic(boolean value) {
        mBackend.setEnableQuic(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder enableSdch(boolean value) {
        // Deprecated and unused by upper layers, do nothing.
        return this;
    }

    @Override
    public ICronetEngineBuilder enableHttp2(boolean value) {
        mBackend.setEnableHttp2(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder enableBrotli(boolean value) {
        mBackend.setEnableBrotli(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder enableHttpCache(int cacheMode, long maxSize) {
        mBackend.setEnableHttpCache(cacheMode, maxSize);
        return this;
    }

    @Override
    public ICronetEngineBuilder addQuicHint(String host, int port, int alternatePort) {
        mBackend.addQuicHint(host, port, alternatePort);
        return this;
    }

    @Override
    public ICronetEngineBuilder addPublicKeyPins(
            String hostName,
            Set<byte[]> pinsSha256,
            boolean includeSubdomains,
            Date expirationDate) {
        mBackend.addPublicKeyPins(
                hostName, pinsSha256, includeSubdomains, expirationDate.toInstant());
        return this;
    }

    @Override
    public ICronetEngineBuilder enablePublicKeyPinningBypassForLocalTrustAnchors(boolean value) {
        mBackend.setEnablePublicKeyPinningBypassForLocalTrustAnchors(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder setThreadPriority(int priority) {
        // not supported by HttpEngine hence implemented in wrapper
        if (priority > Process.THREAD_PRIORITY_LOWEST || priority < -20) {
            throw new IllegalArgumentException("Thread priority invalid");
        }
        mThreadPriority = priority;
        return this;
    }

    @Override
    public ICronetEngineBuilder setExperimentalOptions(String stringOptions) {
        // This only translates known experimental options
        ExperimentalOptions options = new ExperimentalOptions(stringOptions);
        mBackend.setConnectionMigrationOptions(parseConnectionMigrationOptions(options));
        mBackend.setDnsOptions(parseDnsOptions(options));
        mBackend.setQuicOptions(parseQuicOptions(options));
        return this;
    }

    @Override
    public ICronetEngineBuilder enableNetworkQualityEstimator(boolean value) {
        if (!sNQEUnsupportedLogged) {
            Log.i(
                    TAG,
                    "NetworkQualityEstimator is unsupported when HttpEngineNativeProvider is used");
            sNQEUnsupportedLogged = true;
        }
        return this;
    }

    /**
     * Build a {@link CronetEngine} using this builder's configuration.
     *
     * @return constructed {@link CronetEngine}.
     */
    @Override
    public ExperimentalCronetEngine build() {
        return new AndroidHttpEngineWrapper(mBackend.build(), mThreadPriority);
    }

    @VisibleForTesting
    public static android.net.http.ConnectionMigrationOptions parseConnectionMigrationOptions(
            ExperimentalOptions options) {
        android.net.http.ConnectionMigrationOptions.Builder cmOptionsBuilder =
                new android.net.http.ConnectionMigrationOptions.Builder();

        cmOptionsBuilder.setDefaultNetworkMigration(
                optionalBooleanToMigrationOptionState(
                        options.getMigrateSessionsOnNetworkChangeV2Option()));
        cmOptionsBuilder.setPathDegradationMigration(
                optionalBooleanToMigrationOptionState(options.getAllowPortMigration()));

        OptionalBoolean migrateSessionsEarly = options.getMigrateSessionsEarlyV2();
        cmOptionsBuilder.setAllowNonDefaultNetworkUsage(
                optionalBooleanToMigrationOptionState(migrateSessionsEarly));
        if (migrateSessionsEarly == OptionalBoolean.TRUE) {
            cmOptionsBuilder.setPathDegradationMigration(
                    optionalBooleanToMigrationOptionState(OptionalBoolean.TRUE));
        }

        return cmOptionsBuilder.build();
    }

    @VisibleForTesting
    public static android.net.http.DnsOptions parseDnsOptions(ExperimentalOptions options) {
        android.net.http.DnsOptions.StaleDnsOptions.Builder staleDnsOptionBuilder =
                new android.net.http.DnsOptions.StaleDnsOptions.Builder();
        int staleDnsDelay = options.getStaleDnsDelayMillisOption();
        if (staleDnsDelay != ExperimentalOptions.UNSET_INT_VALUE) {
            staleDnsOptionBuilder.setFreshLookupTimeout(Duration.ofMillis(staleDnsDelay));
        }

        int expiredDelay = options.getStaleDnsMaxExpiredTimeMillisOption();
        if (expiredDelay != ExperimentalOptions.UNSET_INT_VALUE) {
            staleDnsOptionBuilder.setMaxExpiredDelay(Duration.ofMillis(expiredDelay));
        }

        staleDnsOptionBuilder
                .setAllowCrossNetworkUsage(
                        optionalBooleanToMigrationOptionState(
                                options.getStaleDnsAllowOtherNetworkOption()))
                .setUseStaleOnNameNotResolved(
                        optionalBooleanToMigrationOptionState(
                                options.getStaleDnsUseStaleOnNameNotResolvedOption()));

        android.net.http.DnsOptions.Builder dnsOptionsBuilder =
                new android.net.http.DnsOptions.Builder();
        dnsOptionsBuilder
                .setUseHttpStackDnsResolver(
                        optionalBooleanToMigrationOptionState(options.getAsyncDnsEnableOption()))
                .setStaleDns(
                        optionalBooleanToMigrationOptionState(options.getStaleDnsEnableOption()))
                .setStaleDnsOptions(staleDnsOptionBuilder.build())
                .setPreestablishConnectionsToStaleDnsResults(
                        optionalBooleanToMigrationOptionState(
                                options.getRaceStaleDnsOnConnection()))
                .setPersistHostCache(
                        optionalBooleanToMigrationOptionState(
                                options.getStaleDnsPersistToDiskOption()));
        int persistHostCachePeriod = options.getStaleDnsPersistDelayMillisOption();
        if (persistHostCachePeriod != ExperimentalOptions.UNSET_INT_VALUE) {
            dnsOptionsBuilder.setPersistHostCachePeriod(Duration.ofMillis(persistHostCachePeriod));
        }

        return dnsOptionsBuilder.build();
    }

    @VisibleForTesting
    public static android.net.http.QuicOptions parseQuicOptions(ExperimentalOptions options) {
        android.net.http.QuicOptions.Builder quicOptionsBuilder =
                new android.net.http.QuicOptions.Builder();

        if (options.getHostWhitelist() != null) {
            for (String host : options.getHostWhitelist().split(",")) {
                quicOptionsBuilder.addAllowedQuicHost(host);
            }
        }

        int inMemoryServerConfigsCacheSize = options.getMaxServerConfigsStoredInPropertiesOption();
        if (inMemoryServerConfigsCacheSize != ExperimentalOptions.UNSET_INT_VALUE) {
            quicOptionsBuilder.setInMemoryServerConfigsCacheSize(inMemoryServerConfigsCacheSize);
        }

        String handshakeUserAgent = options.getUserAgentId();
        if (handshakeUserAgent != null) {
            quicOptionsBuilder.setHandshakeUserAgent(handshakeUserAgent);
        }

        int idleConnectionTimeoutSeconds = options.getIdleConnectionTimeoutSecondsOption();
        if (idleConnectionTimeoutSeconds != ExperimentalOptions.UNSET_INT_VALUE) {
            quicOptionsBuilder.setIdleConnectionTimeout(
                    Duration.ofSeconds(idleConnectionTimeoutSeconds));
        }

        return quicOptionsBuilder.build();
    }

    /**
     * HttpEngine XOptions exposes X_OPTION_* IntDefs that map to the same integer values. To
     * simplify the code, we are reusing ConnectionMigrationOptions.MIGRATION_OPTION_* for
     * DnsOptions and QuicOptions.
     */
    private static int optionalBooleanToMigrationOptionState(OptionalBoolean value) {
        switch (value) {
            case TRUE:
                return android.net.http.ConnectionMigrationOptions.MIGRATION_OPTION_ENABLED;
            case FALSE:
                return android.net.http.ConnectionMigrationOptions.MIGRATION_OPTION_DISABLED;
            case UNSET:
                return android.net.http.ConnectionMigrationOptions.MIGRATION_OPTION_UNSPECIFIED;
        }

        throw new AssertionError("Invalid OptionalBoolean value: " + value);
    }
}
