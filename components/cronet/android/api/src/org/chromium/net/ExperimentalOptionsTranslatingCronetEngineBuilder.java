// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net;

import androidx.annotation.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.net.DnsOptions.StaleDnsOptions;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * An implementation of ICronetEngineBuilder which handles translation of configuration options to
 * json-based experimental options, if necessary.
 *
 * <p>{@hide internal class}
 */
final class ExperimentalOptionsTranslatingCronetEngineBuilder extends ICronetEngineBuilder {
    private static final Set<Integer> SUPPORTED_OPTIONS = Collections.unmodifiableSet(
            new HashSet(Arrays.asList(ICronetEngineBuilder.CONNECTION_MIGRATION_OPTIONS,
                    ICronetEngineBuilder.DNS_OPTIONS, ICronetEngineBuilder.QUIC_OPTIONS)));

    private JSONObject mParsedExperimentalOptions;
    private final List<ExperimentalOptionsPatch> mExperimentalOptionsPatches = new ArrayList<>();

    private final ICronetEngineBuilder mDelegate;

    ExperimentalOptionsTranslatingCronetEngineBuilder(ICronetEngineBuilder delegate) {
        this.mDelegate = delegate;
    }

    @Override
    public ICronetEngineBuilder setQuicOptions(QuicOptions options) {
        // If the delegate builder supports enabling connection migration directly, just use it
        if (mDelegate.getSupportedConfigOptions().contains(ICronetEngineBuilder.QUIC_OPTIONS)) {
            mDelegate.setQuicOptions(options);
            return this;
        }

        // If not, we'll have to work around it by modifying the experimental options JSON.
        mExperimentalOptionsPatches.add((experimentalOptions) -> {
            JSONObject quicOptions = createDefaultIfAbsent(experimentalOptions, "QUIC");

            // Note: using the experimental APIs always overwrites what's in the experimental
            // JSON, even though "repeated" fields could in theory be additive.
            if (!options.getQuicHostAllowlist().isEmpty()) {
                quicOptions.put("host_whitelist", String.join(",", options.getQuicHostAllowlist()));
            }
            if (!options.getEnabledQuicVersions().isEmpty()) {
                quicOptions.put("quic_version", String.join(",", options.getEnabledQuicVersions()));
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
                quicOptions.put("set_quic_flags", String.join(",", options.getExtraQuicheFlags()));
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
    public ICronetEngineBuilder setDnsOptions(DnsOptions options) {
        // If the delegate builder supports enabling connection migration directly, just use it
        if (mDelegate.getSupportedConfigOptions().contains(ICronetEngineBuilder.DNS_OPTIONS)) {
            mDelegate.setDnsOptions(options);
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
                staleDnsOptions.put("persist_delay_ms", options.getPersistHostCachePeriodMillis());
            }

            if (options.getStaleDnsOptions() != null) {
                StaleDnsOptions staleDnsOptionsJava = options.getStaleDnsOptions();

                if (staleDnsOptionsJava.getAllowCrossNetworkUsage() != null) {
                    staleDnsOptions.put(
                            "allow_other_network", staleDnsOptionsJava.getAllowCrossNetworkUsage());
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
                    staleDnsOptions.put(
                            "max_expired_time_ms", staleDnsOptionsJava.getMaxExpiredDelayMillis());
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
    public ICronetEngineBuilder setConnectionMigrationOptions(ConnectionMigrationOptions options) {
        // If the delegate builder supports enabling connection migration directly, just use it
        if (mDelegate.getSupportedConfigOptions().contains(
                    ICronetEngineBuilder.CONNECTION_MIGRATION_OPTIONS)) {
            mDelegate.setConnectionMigrationOptions(options);
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
    public ICronetEngineBuilder setExperimentalOptions(String options) {
        if (options == null || options.isEmpty()) {
            mParsedExperimentalOptions = null;
        } else {
            mParsedExperimentalOptions = parseExperimentalOptions(options);
        }
        return this;
    }

    @Override
    protected Set<Integer> getSupportedConfigOptions() {
        return SUPPORTED_OPTIONS;
    }

    @Override
    public ExperimentalCronetEngine build() {
        if (mParsedExperimentalOptions == null && mExperimentalOptionsPatches.isEmpty()) {
            return mDelegate.build();
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

        mDelegate.setExperimentalOptions(mParsedExperimentalOptions.toString());
        return mDelegate.build();
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

    @VisibleForTesting
    ICronetEngineBuilder getDelegate() {
        return mDelegate;
    }

    @FunctionalInterface
    private interface ExperimentalOptionsPatch {
        void applyTo(JSONObject experimentalOptions) throws JSONException;
    }

    // Delegating-only methods
    @Override
    public ICronetEngineBuilder addPublicKeyPins(String hostName, Set<byte[]> pinsSha256,
            boolean includeSubdomains, Date expirationDate) {
        mDelegate.addPublicKeyPins(hostName, pinsSha256, includeSubdomains, expirationDate);
        return this;
    }

    @Override
    public ICronetEngineBuilder addQuicHint(String host, int port, int alternatePort) {
        mDelegate.addQuicHint(host, port, alternatePort);
        return this;
    }

    @Override
    public ICronetEngineBuilder enableHttp2(boolean value) {
        mDelegate.enableHttp2(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder enableHttpCache(int cacheMode, long maxSize) {
        mDelegate.enableHttpCache(cacheMode, maxSize);
        return this;
    }

    @Override
    public ICronetEngineBuilder enablePublicKeyPinningBypassForLocalTrustAnchors(boolean value) {
        mDelegate.enablePublicKeyPinningBypassForLocalTrustAnchors(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder enableQuic(boolean value) {
        mDelegate.enableQuic(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder enableSdch(boolean value) {
        mDelegate.enableSdch(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder enableBrotli(boolean value) {
        mDelegate.enableBrotli(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder setLibraryLoader(CronetEngine.Builder.LibraryLoader loader) {
        mDelegate.setLibraryLoader(loader);
        return this;
    }

    @Override
    public ICronetEngineBuilder setStoragePath(String value) {
        mDelegate.setStoragePath(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder setUserAgent(String userAgent) {
        mDelegate.setUserAgent(userAgent);
        return this;
    }

    @Override
    public String getDefaultUserAgent() {
        return mDelegate.getDefaultUserAgent();
    }

    @Override
    public ICronetEngineBuilder enableNetworkQualityEstimator(boolean value) {
        mDelegate.enableNetworkQualityEstimator(value);
        return this;
    }

    @Override
    public ICronetEngineBuilder setThreadPriority(int priority) {
        mDelegate.setThreadPriority(priority);
        return this;
    }
}
