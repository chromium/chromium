// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.net.DnsOptions.StaleDnsOptions;

import java.io.IOException;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Executor;

/**
 * {@link CronetEngine} that exposes experimental features. To obtain an instance of this class,
 * cast a {@code CronetEngine} to this type. Every instance of {@code CronetEngine} can be cast to
 * an instance of this class, as they are backed by the same implementation and hence perform
 * identically. Instances of this class are not meant for general use, but instead only to access
 * experimental features. Experimental features may be deprecated in the future. Use at your own
 * risk.
 *
 * <p>{@hide since this class exposes experimental features that should be hidden.}
 *
 * @deprecated scheduled for deletion, don't use in new code.
 */
@Deprecated
public abstract class ExperimentalCronetEngine extends CronetEngine {
    /** The value of a connection metric is unknown. */
    public static final int CONNECTION_METRIC_UNKNOWN = CronetEngine.CONNECTION_METRIC_UNKNOWN;

    /**
     * The estimate of the effective connection type is unknown.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_UNKNOWN =
            CronetEngine.EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

    /**
     * The device is offline.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_OFFLINE =
            CronetEngine.EFFECTIVE_CONNECTION_TYPE_OFFLINE;

    /**
     * The estimate of the effective connection type is slow 2G.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_SLOW_2G =
            CronetEngine.EFFECTIVE_CONNECTION_TYPE_SLOW_2G;

    /**
     * The estimate of the effective connection type is 2G.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_2G =
            CronetEngine.EFFECTIVE_CONNECTION_TYPE_2G;

    /**
     * The estimate of the effective connection type is 3G.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_3G =
            CronetEngine.EFFECTIVE_CONNECTION_TYPE_3G;

    /**
     * The estimate of the effective connection type is 4G.
     *
     * @see #getEffectiveConnectionType
     */
    public static final int EFFECTIVE_CONNECTION_TYPE_4G =
            CronetEngine.EFFECTIVE_CONNECTION_TYPE_4G;

    /** The value to be used to undo any previous network binding. */
    public static final long UNBIND_NETWORK_HANDLE = -1;

    /**
     * A version of {@link CronetEngine.Builder} that exposes experimental features. Instances of
     * this class are not meant for general use, but instead only to access experimental features.
     * Experimental features may be deprecated in the future. Use at your own risk.
     */
    public static class Builder extends CronetEngine.Builder {
        private JSONObject mParsedExperimentalOptions;
        private final List<ExperimentalOptionsPatch> mExperimentalOptionsPatches =
                new ArrayList<>();

        /**
         * Constructs a {@link Builder} object that facilitates creating a {@link CronetEngine}. The
         * default configuration enables HTTP/2 and disables QUIC, SDCH and the HTTP cache.
         *
         * @param context Android {@link Context}, which is used by the Builder to retrieve the
         *     application context. A reference to only the application context will be kept, so as
         * to avoid extending the lifetime of {@code context} unnecessarily.
         */
        public Builder(Context context) {
            super(context);
        }

        /**
         * Constructs {@link Builder} with a given delegate that provides the actual implementation
         * of the {@code Builder} methods. This constructor is used only by the internal
         * implementation.
         *
         * @param builderDelegate delegate that provides the actual implementation.
         *     <p>{@hide}
         */
        public Builder(ICronetEngineBuilder builderDelegate) {
            super(builderDelegate);
        }

        /**
         * Sets experimental options to be used in Cronet.
         *
         * @param options JSON formatted experimental options.
         * @return the builder to facilitate chaining.
         */
        public Builder setExperimentalOptions(String options) {
            if (options == null || options.isEmpty()) {
                mParsedExperimentalOptions = null;
            } else {
                mParsedExperimentalOptions = parseExperimentalOptions(options);
            }
            return this;
        }

        /**
         * Returns delegate, only for testing.
         *
         * @hide
         */
        @VisibleForTesting
        public ICronetEngineBuilder getBuilderDelegate() {
            return mBuilderDelegate;
        }

        // To support method chaining, override superclass methods to return an
        // instance of this class instead of the parent.

        @Override
        public Builder setUserAgent(String userAgent) {
            super.setUserAgent(userAgent);
            return this;
        }

        @Override
        public Builder setStoragePath(String value) {
            super.setStoragePath(value);
            return this;
        }

        @Override
        public Builder setLibraryLoader(LibraryLoader loader) {
            super.setLibraryLoader(loader);
            return this;
        }

        @Override
        public Builder enableQuic(boolean value) {
            super.enableQuic(value);
            return this;
        }

        @Override
        public Builder enableHttp2(boolean value) {
            super.enableHttp2(value);
            return this;
        }

        @Override
        @QuicOptions.Experimental
        public Builder setQuicOptions(QuicOptions options) {
            // If the delegate builder supports enabling connection migration directly, just use it
            if (mBuilderDelegate.getSupportedConfigOptions().contains(
                        ICronetEngineBuilder.QUIC_OPTIONS)) {
                mBuilderDelegate.setQuicOptions(options);
                return this;
            }

            // If not, we'll have to work around it by modifying the experimental options JSON.
            mExperimentalOptionsPatches.add((experimentalOptions) -> {
                JSONObject quicOptions = createDefaultIfAbsent(experimentalOptions, "QUIC");

                // Note: using the experimental APIs always overwrites what's in the experimental
                // JSON, even though "repeated" fields could in theory be additive.
                if (!options.getQuicHostAllowlist().isEmpty()) {
                    quicOptions.put(
                            "host_whitelist", String.join(",", options.getQuicHostAllowlist()));
                }
                if (!options.getEnabledQuicVersions().isEmpty()) {
                    quicOptions.put(
                            "quic_version", String.join(",", options.getEnabledQuicVersions()));
                }
                if (!options.getConnectionOptions().isEmpty()) {
                    quicOptions.put(
                            "connection_options", String.join(",", options.getConnectionOptions()));
                }
                if (!options.getClientConnectionOptions().isEmpty()) {
                    quicOptions.put("client_connection_options",
                            String.join(",", options.getClientConnectionOptions()));
                }
                if (!options.getExtraQuicheFlags().isEmpty()) {
                    quicOptions.put(
                            "set_quic_flags", String.join(",", options.getExtraQuicheFlags()));
                }

                if (options.getInMemoryServerConfigsCacheSize() != null) {
                    quicOptions.put("max_server_configs_stored_in_properties",
                            options.getInMemoryServerConfigsCacheSize());
                }

                if (options.getHandshakeUserAgent() != null) {
                    quicOptions.put("user_agent_id", options.getHandshakeUserAgent());
                }

                if (options.getRetryWithoutAltSvcOnQuicErrors() != null) {
                    quicOptions.put("retry_without_alt_svc_on_quic_errors",
                            options.getRetryWithoutAltSvcOnQuicErrors());
                }

                if (options.getEnableTlsZeroRtt() != null) {
                    quicOptions.put("disable_tls_zero_rtt", !options.getEnableTlsZeroRtt());
                }

                if (options.getPreCryptoHandshakeIdleTimeoutSeconds() != null) {
                    quicOptions.put("max_idle_time_before_crypto_handshake_seconds",
                            options.getPreCryptoHandshakeIdleTimeoutSeconds());
                }

                if (options.getCryptoHandshakeTimeoutSeconds() != null) {
                    quicOptions.put("max_time_before_crypto_handshake_seconds",
                            options.getCryptoHandshakeTimeoutSeconds());
                }

                if (options.getIdleConnectionTimeoutSeconds() != null) {
                    quicOptions.put("idle_connection_timeout_seconds",
                            options.getIdleConnectionTimeoutSeconds());
                }

                if (options.getRetransmittableOnWireTimeoutMillis() != null) {
                    quicOptions.put("retransmittable_on_wire_timeout_milliseconds",
                            options.getRetransmittableOnWireTimeoutMillis());
                }

                if (options.getCloseSessionsOnIpChange() != null) {
                    quicOptions.put(
                            "close_sessions_on_ip_change", options.getCloseSessionsOnIpChange());
                }

                if (options.getGoawaySessionsOnIpChange() != null) {
                    quicOptions.put(
                            "goaway_sessions_on_ip_change", options.getGoawaySessionsOnIpChange());
                }

                if (options.getInitialBrokenServicePeriodSeconds() != null) {
                    quicOptions.put("initial_delay_for_broken_alternative_service_seconds",
                            options.getInitialBrokenServicePeriodSeconds());
                }

                if (options.getIncreaseBrokenServicePeriodExponentially() != null) {
                    quicOptions.put("exponential_backoff_on_initial_delay",
                            options.getIncreaseBrokenServicePeriodExponentially());
                }

                if (options.getDelayJobsWithAvailableSpdySession() != null) {
                    quicOptions.put("delay_main_job_with_available_spdy_session",
                            options.getDelayJobsWithAvailableSpdySession());
                }
            });
            return this;
        }

        @Override
        @DnsOptions.Experimental
        public Builder setDnsOptions(DnsOptions options) {
            // If the delegate builder supports enabling connection migration directly, just use it
            if (mBuilderDelegate.getSupportedConfigOptions().contains(
                        ICronetEngineBuilder.DNS_OPTIONS)) {
                mBuilderDelegate.setDnsOptions(options);
                return this;
            }

            // If not, we'll have to work around it by modifying the experimental options JSON.
            mExperimentalOptionsPatches.add((experimentalOptions) -> {
                JSONObject asyncDnsOptions = createDefaultIfAbsent(experimentalOptions, "AsyncDNS");

                if (options.getUseBuiltInDnsResolver() != null) {
                    asyncDnsOptions.put("enable", options.getUseBuiltInDnsResolver());
                }

                JSONObject staleDnsOptions = createDefaultIfAbsent(experimentalOptions, "StaleDNS");

                if (options.getEnableStaleDns() != null) {
                    staleDnsOptions.put("enable", options.getEnableStaleDns());
                }

                if (options.getPersistHostCache() != null) {
                    staleDnsOptions.put("persist_to_disk", options.getPersistHostCache());
                }

                if (options.getPersistHostCachePeriodMillis() != null) {
                    staleDnsOptions.put(
                            "persist_delay_ms", options.getPersistHostCachePeriodMillis());
                }

                if (options.getStaleDnsOptions() != null) {
                    StaleDnsOptions staleDnsOptionsJava = options.getStaleDnsOptions();

                    if (staleDnsOptionsJava.getAllowCrossNetworkUsage() != null) {
                        staleDnsOptions.put("allow_other_network",
                                staleDnsOptionsJava.getAllowCrossNetworkUsage());
                    }

                    if (staleDnsOptionsJava.getFreshLookupTimeoutMillis() != null) {
                        staleDnsOptions.put(
                                "delay_ms", staleDnsOptionsJava.getFreshLookupTimeoutMillis());
                    }

                    if (staleDnsOptionsJava.getUseStaleOnNameNotResolved() != null) {
                        staleDnsOptions.put("use_stale_on_name_not_resolved",
                                staleDnsOptionsJava.getUseStaleOnNameNotResolved());
                    }

                    if (staleDnsOptionsJava.getMaxExpiredDelayMillis() != null) {
                        staleDnsOptions.put("max_expired_time_ms",
                                staleDnsOptionsJava.getMaxExpiredDelayMillis());
                    }
                }

                JSONObject quicOptions = createDefaultIfAbsent(experimentalOptions, "QUIC");
                if (options.getPreestablishConnectionsToStaleDnsResults() != null) {
                    quicOptions.put("race_stale_dns_on_connection",
                            options.getPreestablishConnectionsToStaleDnsResults());
                }
            });
            return this;
        }

        @Override
        @ConnectionMigrationOptions.Experimental
        public Builder setConnectionMigrationOptions(ConnectionMigrationOptions options) {
            // If the delegate builder supports enabling connection migration directly, just use it
            if (mBuilderDelegate.getSupportedConfigOptions().contains(
                        ICronetEngineBuilder.CONNECTION_MIGRATION_OPTIONS)) {
                mBuilderDelegate.setConnectionMigrationOptions(options);
                return this;
            }

            // If not, we'll have to work around it by modifying the experimental options JSON.
            mExperimentalOptionsPatches.add((experimentalOptions) -> {
                JSONObject quicOptions = createDefaultIfAbsent(experimentalOptions, "QUIC");

                if (options.getEnableDefaultNetworkMigration() != null) {
                    quicOptions.put("migrate_sessions_on_network_change_v2",
                            options.getEnableDefaultNetworkMigration());
                }
                if (options.getAllowServerMigration() != null) {
                    quicOptions.put("allow_server_migration", options.getAllowServerMigration());
                }
                if (options.getMigrateIdleConnections() != null) {
                    quicOptions.put("migrate_idle_sessions", options.getMigrateIdleConnections());
                }
                if (options.getIdleMigrationPeriodSeconds() != null) {
                    quicOptions.put("idle_session_migration_period_seconds",
                            options.getIdleMigrationPeriodSeconds());
                }
                if (options.getRetryPreHandshakeErrorsOnAlternateNetwork() != null) {
                    quicOptions.put("retry_on_alternate_network_before_handshake",
                            options.getRetryPreHandshakeErrorsOnAlternateNetwork());
                }
                if (options.getMaxTimeOnNonDefaultNetworkSeconds() != null) {
                    quicOptions.put("max_time_on_non_default_network_seconds",
                            options.getMaxTimeOnNonDefaultNetworkSeconds());
                }
                if (options.getMaxPathDegradingEagerMigrationsCount() != null) {
                    quicOptions.put("max_migrations_to_non_default_network_on_path_degrading",
                            options.getMaxPathDegradingEagerMigrationsCount());
                }
                if (options.getMaxWriteErrorEagerMigrationsCount() != null) {
                    quicOptions.put("max_migrations_to_non_default_network_on_write_error",
                            options.getMaxWriteErrorEagerMigrationsCount());
                }
                if (options.getEnablePathDegradationMigration() != null) {
                    boolean pathDegradationValue = options.getEnablePathDegradationMigration();

                    boolean skipPortMigrationFlag = false;

                    if (options.getAllowNonDefaultNetworkUsage() != null) {
                        boolean nonDefaultNetworkValue = options.getAllowNonDefaultNetworkUsage();
                        if (!pathDegradationValue && nonDefaultNetworkValue) {
                            // Misconfiguration which doesn't translate easily to the JSON flags
                            throw new IllegalArgumentException(
                                    "Unable to turn on non-default network usage without path "
                                    + "degradation migration!");
                        } else if (pathDegradationValue && nonDefaultNetworkValue) {
                            // Both values being true results in the non-default network migration
                            // being enabled.
                            quicOptions.put("migrate_sessions_early_v2", true);
                            skipPortMigrationFlag = true;
                        } else {
                            quicOptions.put("migrate_sessions_early_v2", false);
                        }
                    }

                    if (!skipPortMigrationFlag) {
                        quicOptions.put("allow_port_migration", pathDegradationValue);
                    }
                }
            });

            return this;
        }

        @Override
        public Builder enableSdch(boolean value) {
            return this;
        }

        @Override
        public Builder enableHttpCache(int cacheMode, long maxSize) {
            super.enableHttpCache(cacheMode, maxSize);
            return this;
        }

        @Override
        public Builder addQuicHint(String host, int port, int alternatePort) {
            super.addQuicHint(host, port, alternatePort);
            return this;
        }

        @Override
        public Builder addPublicKeyPins(String hostName, Set<byte[]> pinsSha256,
                boolean includeSubdomains, Date expirationDate) {
            super.addPublicKeyPins(hostName, pinsSha256, includeSubdomains, expirationDate);
            return this;
        }

        @Override
        public Builder enablePublicKeyPinningBypassForLocalTrustAnchors(boolean value) {
            super.enablePublicKeyPinningBypassForLocalTrustAnchors(value);
            return this;
        }

        @Override
        public Builder enableNetworkQualityEstimator(boolean value) {
            super.enableNetworkQualityEstimator(value);
            return this;
        }

        @Override
        public Builder setThreadPriority(int priority) {
            super.setThreadPriority(priority);
            return this;
        }

        @Override
        public ExperimentalCronetEngine build() {
            if (mParsedExperimentalOptions == null && mExperimentalOptionsPatches.isEmpty()) {
                return mBuilderDelegate.build();
            }

            if (mParsedExperimentalOptions == null) {
                mParsedExperimentalOptions = new JSONObject();
            }

            for (ExperimentalOptionsPatch patch : mExperimentalOptionsPatches) {
                try {
                    patch.applyTo(mParsedExperimentalOptions);
                } catch (JSONException e) {
                    throw new IllegalStateException("Unable to apply JSON patch!", e);
                }
            }

            mBuilderDelegate.setExperimentalOptions(mParsedExperimentalOptions.toString());
            return mBuilderDelegate.build();
        }

        private static JSONObject parseExperimentalOptions(String jsonString) {
            try {
                return new JSONObject(jsonString);
            } catch (JSONException e) {
                throw new IllegalArgumentException("Experimental options parsing failed", e);
            }
        }

        private static JSONObject createDefaultIfAbsent(JSONObject jsonObject, String key) {
            JSONObject object = jsonObject.optJSONObject(key);
            if (object == null) {
                object = new JSONObject();
                try {
                    jsonObject.put(key, object);
                } catch (JSONException e) {
                    throw new IllegalArgumentException(
                            "Failed adding a default object for key [" + key + "]", e);
                }
            }

            return object;
        }

        @FunctionalInterface
        private interface ExperimentalOptionsPatch {
            void applyTo(JSONObject experimentalOptions) throws JSONException;
        }
    }

    @Override
    public abstract ExperimentalUrlRequest.Builder newUrlRequestBuilder(
            String url, UrlRequest.Callback callback, Executor executor);

    /**
     * Creates a builder for {@link BidirectionalStream} objects. All callbacks for
     * generated {@code BidirectionalStream} objects will be invoked on
     * {@code executor}. {@code executor} must not run tasks on the
     * current thread, otherwise the networking operations may block and exceptions
     * may be thrown at shutdown time.
     *
     * @param url URL for the generated streams.
     * @param callback the {@link BidirectionalStream.Callback} object that gets invoked upon
     * different events occurring.
     * @param executor the {@link Executor} on which {@code callback} methods will be invoked.
     *
     * @return the created builder.
     */
    public abstract ExperimentalBidirectionalStream.Builder newBidirectionalStreamBuilder(
            String url, BidirectionalStream.Callback callback, Executor executor);

    /**
     * Binds the engine to the specified network handle. All requests created through this engine
     * will use the network associated to this handle. If this network disconnects all requests will
     * fail, the exact error will depend on the stage of request processing when the network
     * disconnects. Network handles can be obtained through {@code Network#getNetworkHandle}. Only
     * available starting from Android Marshmallow.
     *
     * @param networkHandle the network handle to bind the engine to. Specify {@link
     * #UNBIND_NETWORK_HANDLE} to unbind.
     */
    public void bindToNetwork(long networkHandle) {}

    /**
     * Establishes a new connection to the resource specified by the {@link URL} {@code url} using
     * the given proxy. <p> <b>Note:</b> Cronet's {@link java.net.HttpURLConnection} implementation
     * is subject to certain limitations, see {@link #createURLStreamHandlerFactory} for details.
     *
     * @param url URL of resource to connect to.
     * @param proxy proxy to use when establishing connection.
     * @return an {@link java.net.HttpURLConnection} instance implemented by this CronetEngine.
     * @throws IOException if an error occurs while opening the connection.
     */
    // TODO(pauljensen): Expose once implemented, http://crbug.com/418111
    public URLConnection openConnection(URL url, Proxy proxy) throws IOException {
        return url.openConnection(proxy);
    }
}
