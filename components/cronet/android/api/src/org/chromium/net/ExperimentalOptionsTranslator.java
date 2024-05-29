// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.net;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.net.DnsOptions.StaleDnsOptions;

import java.util.List;

/**
 * A helper class to translate the experimental options to other formats(eg json) and vice versa.
 */
final class ExperimentalOptionsTranslator {
    private ExperimentalOptionsTranslator() {}

    static void connectionMigrationOptionsToJson(
            JSONObject experimentalOptions, ConnectionMigrationOptions options)
            throws JSONException {
        JSONObject quicOptions = createDefaultIfAbsent(experimentalOptions, "QUIC");

        if (options.getEnableDefaultNetworkMigration() != null) {
            quicOptions.put(
                    "migrate_sessions_on_network_change_v2",
                    options.getEnableDefaultNetworkMigration());
        }
        if (options.getAllowServerMigration() != null) {
            quicOptions.put("allow_server_migration", options.getAllowServerMigration());
        }
        if (options.getMigrateIdleConnections() != null) {
            quicOptions.put("migrate_idle_sessions", options.getMigrateIdleConnections());
        }
        if (options.getIdleMigrationPeriodSeconds() != null) {
            quicOptions.put(
                    "idle_session_migration_period_seconds",
                    options.getIdleMigrationPeriodSeconds());
        }
        if (options.getRetryPreHandshakeErrorsOnAlternateNetwork() != null) {
            quicOptions.put(
                    "retry_on_alternate_network_before_handshake",
                    options.getRetryPreHandshakeErrorsOnAlternateNetwork());
        }
        if (options.getMaxTimeOnNonDefaultNetworkSeconds() != null) {
            quicOptions.put(
                    "max_time_on_non_default_network_seconds",
                    options.getMaxTimeOnNonDefaultNetworkSeconds());
        }
        if (options.getMaxPathDegradingEagerMigrationsCount() != null) {
            quicOptions.put(
                    "max_migrations_to_non_default_network_on_path_degrading",
                    options.getMaxPathDegradingEagerMigrationsCount());
        }
        if (options.getMaxWriteErrorEagerMigrationsCount() != null) {
            quicOptions.put(
                    "max_migrations_to_non_default_network_on_write_error",
                    options.getMaxWriteErrorEagerMigrationsCount());
        }
        if (options.getEnablePathDegradationMigration() != null) {
            boolean pathDegradationValue = options.getEnablePathDegradationMigration();
            quicOptions.put("allow_port_migration", pathDegradationValue);

            if (options.getAllowNonDefaultNetworkUsage() != null) {
                boolean nonDefaultNetworkValue = options.getAllowNonDefaultNetworkUsage();
                if (!pathDegradationValue && nonDefaultNetworkValue) {
                    // Misconfiguration which doesn't translate easily to the JSON flags
                    throw new IllegalArgumentException(
                            "Unable to turn on non-default network usage without path "
                                    + "degradation migration!");
                } else if (pathDegradationValue && nonDefaultNetworkValue) {
                    quicOptions.put("migrate_sessions_early_v2", true);
                    // To enable connection migration,
                    // migrate_sessions_on_network_change_v2 option needs to be set.
                    // See http://go/quic-cmv2-config-options or
                    // https://crsrc.org/c/net/quic/quic_session_pool.cc;l=1532;drc=4a6bf24b15fdb49a018a12af2025321691f87e1a?q=migrate_sessions_on_network_change
                    quicOptions.put("migrate_sessions_on_network_change_v2", true);
                } else {
                    quicOptions.put("migrate_sessions_early_v2", false);
                }
            }
        }
    }

    static void dnsOptionsToJson(JSONObject experimentalOptions, DnsOptions options)
            throws JSONException {
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
                staleDnsOptions.put("delay_ms", staleDnsOptionsJava.getFreshLookupTimeoutMillis());
            }

            if (staleDnsOptionsJava.getUseStaleOnNameNotResolved() != null) {
                staleDnsOptions.put(
                        "use_stale_on_name_not_resolved",
                        staleDnsOptionsJava.getUseStaleOnNameNotResolved());
            }

            if (staleDnsOptionsJava.getMaxExpiredDelayMillis() != null) {
                staleDnsOptions.put(
                        "max_expired_time_ms", staleDnsOptionsJava.getMaxExpiredDelayMillis());
            }
        }

        JSONObject quicOptions = createDefaultIfAbsent(experimentalOptions, "QUIC");
        if (options.getPreestablishConnectionsToStaleDnsResults() != null) {
            quicOptions.put(
                    "race_stale_dns_on_connection",
                    options.getPreestablishConnectionsToStaleDnsResults());
        }
    }

    static void quicOptionsToJson(JSONObject experimentalOptions, QuicOptions options)
            throws JSONException {
        JSONObject quicOptions = createDefaultIfAbsent(experimentalOptions, "QUIC");

        // Note: using the experimental APIs always overwrites what's in the
        // experimental JSON, even though "repeated" fields could in theory be
        // additive.
        if (!options.getQuicHostAllowlist().isEmpty()) {
            quicOptions.put("host_whitelist", String.join(",", options.getQuicHostAllowlist()));
        }
        if (!options.getEnabledQuicVersions().isEmpty()) {
            quicOptions.put("quic_version", String.join(",", options.getEnabledQuicVersions()));
        }
        if (!options.getConnectionOptions().isEmpty()) {
            quicOptions.put("connection_options", String.join(",", options.getConnectionOptions()));
        }
        if (!options.getClientConnectionOptions().isEmpty()) {
            quicOptions.put(
                    "client_connection_options",
                    String.join(",", options.getClientConnectionOptions()));
        }
        if (!options.getExtraQuicheFlags().isEmpty()) {
            quicOptions.put("set_quic_flags", String.join(",", options.getExtraQuicheFlags()));
        }

        if (options.getInMemoryServerConfigsCacheSize() != null) {
            quicOptions.put(
                    "max_server_configs_stored_in_properties",
                    options.getInMemoryServerConfigsCacheSize());
        }

        if (options.getHandshakeUserAgent() != null) {
            quicOptions.put("user_agent_id", options.getHandshakeUserAgent());
        }

        if (options.getRetryWithoutAltSvcOnQuicErrors() != null) {
            quicOptions.put(
                    "retry_without_alt_svc_on_quic_errors",
                    options.getRetryWithoutAltSvcOnQuicErrors());
        }

        if (options.getEnableTlsZeroRtt() != null) {
            quicOptions.put("disable_tls_zero_rtt", !options.getEnableTlsZeroRtt());
        }

        if (options.getPreCryptoHandshakeIdleTimeoutSeconds() != null) {
            quicOptions.put(
                    "max_idle_time_before_crypto_handshake_seconds",
                    options.getPreCryptoHandshakeIdleTimeoutSeconds());
        }

        if (options.getCryptoHandshakeTimeoutSeconds() != null) {
            quicOptions.put(
                    "max_time_before_crypto_handshake_seconds",
                    options.getCryptoHandshakeTimeoutSeconds());
        }

        if (options.getIdleConnectionTimeoutSeconds() != null) {
            quicOptions.put(
                    "idle_connection_timeout_seconds", options.getIdleConnectionTimeoutSeconds());
        }

        if (options.getRetransmittableOnWireTimeoutMillis() != null) {
            quicOptions.put(
                    "retransmittable_on_wire_timeout_milliseconds",
                    options.getRetransmittableOnWireTimeoutMillis());
        }

        if (options.getCloseSessionsOnIpChange() != null) {
            quicOptions.put("close_sessions_on_ip_change", options.getCloseSessionsOnIpChange());
        }

        if (options.getGoawaySessionsOnIpChange() != null) {
            quicOptions.put("goaway_sessions_on_ip_change", options.getGoawaySessionsOnIpChange());
        }

        if (options.getInitialBrokenServicePeriodSeconds() != null) {
            quicOptions.put(
                    "initial_delay_for_broken_alternative_service_seconds",
                    options.getInitialBrokenServicePeriodSeconds());
        }

        if (options.getIncreaseBrokenServicePeriodExponentially() != null) {
            quicOptions.put(
                    "exponential_backoff_on_initial_delay",
                    options.getIncreaseBrokenServicePeriodExponentially());
        }

        if (options.getDelayJobsWithAvailableSpdySession() != null) {
            quicOptions.put(
                    "delay_main_job_with_available_spdy_session",
                    options.getDelayJobsWithAvailableSpdySession());
        }
    }

    static JSONObject toJsonExperimentalOptions(String jsonString) {
        if (jsonString == null || jsonString.isEmpty()) {
            return null;
        }
        try {
            return new JSONObject(jsonString);
        } catch (JSONException e) {
            throw new IllegalArgumentException("Experimental options parsing failed", e);
        }
    }

    /**
     * Applies the json patches to the json object.
     *
     * @param jsonOptions The JSONObject to apply the patches to.
     * @param patches The list of patches to apply to the JSONObject
     * @return The JSONObject or a new JSONObject if the provided object is null, with the patches
     *     applied.
     */
    static JSONObject applyJsonPatches(JSONObject jsonOptions, List<JsonPatch> patches) {
        if (jsonOptions == null && patches.isEmpty()) {
            return null;
        }

        if (jsonOptions == null) {
            jsonOptions = new JSONObject();
        }

        for (JsonPatch patch : patches) {
            try {
                patch.applyTo(jsonOptions);
            } catch (JSONException e) {
                throw new IllegalStateException("Unable to apply JSON patch!", e);
            }
        }

        return jsonOptions;
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

    /** Represents a function that modifies the given JSON object */
    @FunctionalInterface
    interface JsonPatch {
        void applyTo(JSONObject experimentalOptions) throws JSONException;
    }
}
