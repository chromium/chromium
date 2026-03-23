// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.telemetry;

import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;

import java.util.ArrayList;
import java.util.Locale;
import java.util.Set;

/** Parses the experimentalOptions string */
public final class ExperimentalOptions {
    private static final String TAG = ExperimentalOptions.class.getSimpleName();

    public static final int UNSET_INT_VALUE = -1;

    // Declare static experimental options field trial names
    private static final String QUIC = "QUIC";
    private static final String ASYNC_DNS = "AsyncDNS";
    private static final String STALE_DNS = "StaleDNS";

    // The JSONObject created from the experimentalOptions String
    private JSONObject mJson = new JSONObject();

    public ExperimentalOptions(String experimentalOptions) {
        if (!isNullOrEmpty(experimentalOptions)) {
            try {
                mJson = (JSONObject) new JSONTokener(experimentalOptions).nextValue();
            } catch (JSONException | ClassCastException e) {
                if (Log.isLoggable(TAG, Log.VERBOSE)) {
                    Log.v(
                            TAG,
                            String.format(
                                    "Experimental options could not be parsed, using default"
                                            + " values. Error: %s",
                                    e.getMessage()));
                }
            }
        }
    }

    public String getConnectionOptionsOption() {
        return parseExperimentalOptionsString(
                getOrDefault(QUIC, "connection_options", null, String.class));
    }

    public OptionalBoolean getStoreServerConfigsInPropertiesOption() {
        return OptionalBoolean.fromBoolean(
                getOrDefault(QUIC, "store_server_configs_in_properties", null, Boolean.class));
    }

    public int getMaxServerConfigsStoredInPropertiesOption() {
        return getOrDefault(
                QUIC, "max_server_configs_stored_in_properties", UNSET_INT_VALUE, Integer.class);
    }

    @SuppressWarnings("GoodTime") // CronetStatsLog expects int
    public int getIdleConnectionTimeoutSecondsOption() {
        return getOrDefault(
                QUIC, "idle_connection_timeout_seconds", UNSET_INT_VALUE, Integer.class);
    }

    public OptionalBoolean getGoawaySessionsOnIpChangeOption() {
        return OptionalBoolean.fromBoolean(
                getOrDefault(QUIC, "goaway_sessions_on_ip_change", null, Boolean.class));
    }

    public OptionalBoolean getCloseSessionsOnIpChangeOption() {
        return OptionalBoolean.fromBoolean(
                getOrDefault(QUIC, "close_sessions_on_ip_change", null, Boolean.class));
    }

    public OptionalBoolean getMigrateSessionsOnNetworkChangeV2Option() {
        return OptionalBoolean.fromBoolean(
                getOrDefault(QUIC, "migrate_sessions_on_network_change_v2", null, Boolean.class));
    }

    public OptionalBoolean getMigrateSessionsEarlyV2() {
        return OptionalBoolean.fromBoolean(
                getOrDefault(QUIC, "migrate_sessions_early_v2", null, Boolean.class));
    }

    public OptionalBoolean getDisableBidirectionalStreamsOption() {
        return OptionalBoolean.fromBoolean(
                getOrDefault(QUIC, "disable_bidirectional_streams", null, Boolean.class));
    }

    @SuppressWarnings("GoodTime") // CronetStatsLog expects int
    public int getMaxTimeBeforeCryptoHandshakeSecondsOption() {
        return getOrDefault(
                QUIC, "max_time_before_crypto_handshake_seconds", UNSET_INT_VALUE, Integer.class);
    }

    @SuppressWarnings("GoodTime") // CronetStatsLog expects int
    public int getMaxIdleTimeBeforeCryptoHandshakeSecondsOption() {
        return getOrDefault(
                QUIC,
                "max_idle_time_before_crypto_handshake_seconds",
                UNSET_INT_VALUE,
                Integer.class);
    }

    public OptionalBoolean getEnableSocketRecvOptimizationOption() {
        return OptionalBoolean.fromBoolean(
                getOrDefault(QUIC, "enable_socket_recv_optimization", null, Boolean.class));
    }

    public OptionalBoolean getAllowPortMigration() {
        return OptionalBoolean.fromBoolean(
                getOrDefault(QUIC, "allow_port_migration", null, Boolean.class));
    }

    public OptionalBoolean getRaceStaleDnsOnConnection() {
        return OptionalBoolean.fromBoolean(
                getOrDefault(QUIC, "race_stale_dns_on_connection", null, Boolean.class));
    }

    public String getHostWhitelist() {
        return getOrDefault(QUIC, "host_whitelist", null, String.class);
    }

    public String getUserAgentId() {
        return getOrDefault(QUIC, "user_agent_id", null, String.class);
    }

    public OptionalBoolean getAsyncDnsEnableOption() {
        return OptionalBoolean.fromBoolean(getOrDefault(ASYNC_DNS, "enable", null, Boolean.class));
    }

    public OptionalBoolean getStaleDnsEnableOption() {
        return OptionalBoolean.fromBoolean(getOrDefault(STALE_DNS, "enable", null, Boolean.class));
    }

    @SuppressWarnings("GoodTime") // CronetStatsLog expects int
    public int getStaleDnsDelayMillisOption() {
        return getOrDefault(STALE_DNS, "delay_ms", UNSET_INT_VALUE, Integer.class);
    }

    @SuppressWarnings("GoodTime") // CronetStatsLog expects int
    public int getStaleDnsMaxExpiredTimeMillisOption() {
        return getOrDefault(STALE_DNS, "max_expired_time_ms", UNSET_INT_VALUE, Integer.class);
    }

    public int getStaleDnsMaxStaleUsesOption() {
        return getOrDefault(STALE_DNS, "max_stale_uses", UNSET_INT_VALUE, Integer.class);
    }

    public OptionalBoolean getStaleDnsAllowOtherNetworkOption() {
        return OptionalBoolean.fromBoolean(
                getOrDefault(STALE_DNS, "allow_other_network", null, Boolean.class));
    }

    public OptionalBoolean getStaleDnsPersistToDiskOption() {
        return OptionalBoolean.fromBoolean(
                getOrDefault(STALE_DNS, "persist_to_disk", null, Boolean.class));
    }

    @SuppressWarnings("GoodTime") // CronetStatsLog expects int
    public int getStaleDnsPersistDelayMillisOption() {
        return getOrDefault(STALE_DNS, "persist_delay_ms", UNSET_INT_VALUE, Integer.class);
    }

    public OptionalBoolean getStaleDnsUseStaleOnNameNotResolvedOption() {
        return OptionalBoolean.fromBoolean(
                getOrDefault(STALE_DNS, "use_stale_on_name_not_resolved", null, Boolean.class));
    }

    public OptionalBoolean getDisableIpv6OnWifiOption() {
        return OptionalBoolean.fromBoolean(
                getOrDefault("disable_ipv6_on_wifi", null, Boolean.class));
    }

    /**
     * Checks if an experimentalOption fieldTrial key exists, then gets the value of the child
     * option.
     *
     * @param experimentalOptionFieldTrialName the super option name for a nested experimental
     *     option eg QUIC.connection_options where <code>QUIC</code> is the FieldTrialName and
     *     <code>
     *     connection_options</code> is the child option
     * @param option the child option eg <code>connection_options</code>
     * @param defaultValue the defaultValue if the option is null or empty
     * @return the experimental option value
     */
    private <T> T getOrDefault(
            String experimentalOptionFieldTrialName,
            String option,
            T defaultValue,
            Class<T> clazz) {
        if (mJson.length() == 0) {
            return defaultValue;
        }

        // check if the field trial name exists
        JSONObject options = null;
        try {
            options = mJson.getJSONObject(experimentalOptionFieldTrialName);
        } catch (JSONException e) {
            if (Log.isLoggable(TAG, Log.VERBOSE)) {
                Log.v(
                        TAG,
                        String.format(
                                "Failed to get %s options: %s",
                                experimentalOptionFieldTrialName, e.getMessage()));
            }
        }

        if (options == null || options.length() == 0) {
            return defaultValue;
        }

        T value = defaultValue;
        try {
            value = clazz.cast(options.get(option));
        } catch (JSONException | ClassCastException e) {
            if (Log.isLoggable(TAG, Log.VERBOSE)) {
                Log.v(TAG, String.format("Failed to get %s options: %s", option, e.getMessage()));
            }
        }
        return value;
    }

    private <T> T getOrDefault(String option, T defaultValue, Class<T> clazz) {
        if (mJson.length() == 0) {
            return defaultValue;
        }

        T value = defaultValue;
        try {
            value = clazz.cast(mJson.get(option));
        } catch (JSONException | ClassCastException e) {
            if (Log.isLoggable(TAG, Log.VERBOSE)) {
                Log.v(TAG, String.format("Failed to get %s options: %s", option, e.getMessage()));
            }
        }
        return value;
    }

    /**
     * Checks that the connection_options options are always valid and do not contain any PII.
     * Removes any value that does not conform to a valid option.
     */
    private String parseExperimentalOptionsString(String str) {
        if (isNullOrEmpty(str)) {
            return str;
        }

        ArrayList<String> nStr = new ArrayList<>();
        for (String s : str.split(",", -1)) {
            if (VALID_CONNECTION_OPTIONS.contains(s.toUpperCase(Locale.ROOT).trim())) {
                nStr.add(s);
            }
        }

        return String.join(",", nStr);
    }

    private boolean isNullOrEmpty(String str) {
        return str == null || str.isEmpty();
    }

    // Source:
    // //external/cronet:net/third_party/quiche/src/quiche/quic/core/crypto/crypto_protocol.h
    public static final Set<String> VALID_CONNECTION_OPTIONS =
            Set.of(
                    "CHLO", "SHLO", "SCFG", "REJ", "CETV", "PRST", "SCUP", "ALPN", "P256", "C255",
                    "AESG", "CC20", "QBIC", "AFCW", "IFW5", "IFW6", "IFW7", "IFW8", "IFW9", "IFWA",
                    "TBBR", "1RTT", "2RTT", "LRTT", "BBS1", "BBS2", "BBS3", "BBS4", "BBS5", "BBRR",
                    "BBR1", "BBR2", "BBR3", "BBR4", "BBR5", "BBR9", "BBRA", "BBRB", "BBRS", "BBQ1",
                    "BBQ2", "BBQ3", "BBQ5", "BBQ6", "BBQ7", "BBQ8", "BBQ9", "BBQ0", "RENO", "TPCC",
                    "BYTE", "IW03", "IW10", "IW20", "IW50", "B2ON", "B2NA", "B2NE", "B2RP", "B2LO",
                    "B2HR", "B2SL", "B2H2", "B2RC", "BSAO", "B2DL", "B201", "B202", "B203", "B204",
                    "B205", "B206", "B207", "NTLP", "1TLP", "1RTO", "NRTO", "TIME", "ATIM", "MIN1",
                    "MIN4", "MAD0", "MAD2", "MAD3", "1ACK", "AKD3", "AKDU", "AFFE", "AFF1", "AFF2",
                    "SSLR", "NPRR", "2RTO", "3RTO", "4RTO", "5RTO", "6RTO", "CBHD", "NBHD", "CONH",
                    "LFAK", "STMP", "EACK", "ILD0", "ILD1", "ILD2", "ILD3", "ILD4", "RUNT", "NSTP",
                    "NRTT", "1PTO", "2PTO", "6PTO", "7PTO", "8PTO", "PTOS", "PTOA", "PEB1", "PEB2",
                    "PVS1", "PAG1", "PAG2", "PSDA", "PLE1", "PLE2", "APTO", "ELDT", "RVCM", "TCID",
                    "MPTH", "NCMR", "DFER", "NPCO", "BWRE", "BWMX", "BWID", "BWI1", "BWRS", "BWS2",
                    "BWS3", "BWS4", "BWS5", "BWS6", "BWP0", "BWP1", "BWP2", "BWP3", "BWP4", "BWG4",
                    "BWG7", "BWG8", "BWS7", "BWM3", "BWM4", "ICW1", "DTOS", "FIDT", "3AFF", "10AF",
                    "MTUH", "MTUL", "NSLC", "NCHP", "NBPE", "X509", "X59R", "CHID", "VER ", "NONC",
                    "NONP", "KEXS", "AEAD", "COPT", "CLOP", "ICSL", "MIBS", "MIUS", "ADE ", "IRTT",
                    "TRTT", "SNI ", "PUBS", "SCID", "ORBT", "PDMD", "PROF", "CCRT", "EXPY", "STTL",
                    "SFCW", "CFCW", "UAID", "XLCT", "QLVE", "PDP1", "PDP2", "PDP3", "PDP5", "QNZ2",
                    "MAD", "IGNP", "SRWP", "ROWF", "ROWR", "GSR0", "GSR1", "GSR2", "GSR3", "NRES",
                    "INVC", "GWCH", "YTCH", "ACH0", "RREJ", "CADR", "ASAD", "SRST", "CIDK", "CIDS",
                    "RNON", "RSEQ", "PAD ", "EPID", "SNO0", "STK0", "CRT255", "CSCT");
}
