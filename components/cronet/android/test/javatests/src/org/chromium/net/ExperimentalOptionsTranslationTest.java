// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertThrows;

import androidx.annotation.OptIn;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;

import org.jni_zero.JNINamespace;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.DnsOptions.StaleDnsOptions;

import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

@DoNotBatch(reason = "crbug/1459563")
@RunWith(AndroidJUnit4.class)
@JNINamespace("cronet")
@OptIn(
        markerClass = {
            ConnectionMigrationOptions.Experimental.class,
            DnsOptions.Experimental.class,
            QuicOptions.Experimental.class,
            QuicOptions.QuichePassthroughOption.class
        })
public class ExperimentalOptionsTranslationTest {
    private static final String EXPECTED_CONNECTION_MIGRATION_ENABLED_STRING =
            "{\"QUIC\":{\"migrate_sessions_on_network_change_v2\":true}}";

    @Test
    @MediumTest
    public void testEnableDefaultNetworkConnectionMigrationApi_noBuilderSupport() {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        CronetEngine.Builder builder = new CronetEngine.Builder(mockBuilderImpl);

        builder.setConnectionMigrationOptions(
                ConnectionMigrationOptions.builder().enableDefaultNetworkMigration(true));
        builder.build();

        assertThat(mockBuilderImpl.mConnectionMigrationOptions).isNull();
        assertJsonEquals(
                EXPECTED_CONNECTION_MIGRATION_ENABLED_STRING,
                mockBuilderImpl.mEffectiveExperimentalOptions);
    }

    @Test
    @MediumTest
    public void enableDefaultNetworkConnectionMigrationApi_builderSupport() {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withNativeSetterSupport();
        CronetEngine.Builder builder = new CronetEngine.Builder(mockBuilderImpl);

        builder.setConnectionMigrationOptions(
                ConnectionMigrationOptions.builder().enableDefaultNetworkMigration(true));
        builder.build();

        assertThat(mockBuilderImpl.mConnectionMigrationOptions.getEnableDefaultNetworkMigration())
                .isTrue();
        assertThat(mockBuilderImpl.mEffectiveExperimentalOptions).isNull();
    }

    @Test
    @MediumTest
    public void
            testEnableDefaultNetworkConnectionMigrationApi_noBuilderSupport_setterTakesPrecedence() {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        // This test must instantiate an ExperimentalCronetEngine.Builder since we want to call
        // setExperimentalOptions. We still cast it down to CronetEngine.Builder to confirm
        // things work properly when using that (see crbug/1448520).
        CronetEngine.Builder builder = new ExperimentalCronetEngine.Builder(mockBuilderImpl);

        builder.setConnectionMigrationOptions(
                ConnectionMigrationOptions.builder().enableDefaultNetworkMigration(true));
        ((ExperimentalCronetEngine.Builder) builder)
                .setExperimentalOptions(
                        "{\"QUIC\": {\"migrate_sessions_on_network_change_v2\": false}}");
        builder.build();

        assertThat(mockBuilderImpl.mConnectionMigrationOptions).isNull();
        assertJsonEquals(
                EXPECTED_CONNECTION_MIGRATION_ENABLED_STRING,
                mockBuilderImpl.mEffectiveExperimentalOptions);
    }

    @Test
    @MediumTest
    public void testEnablePathDegradingConnectionMigration_justNonDefaultNetwork() {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        CronetEngine.Builder builder = new CronetEngine.Builder(mockBuilderImpl);

        builder.setConnectionMigrationOptions(
                ConnectionMigrationOptions.builder().allowNonDefaultNetworkUsage(true));
        builder.build();

        assertThat(mockBuilderImpl.mConnectionMigrationOptions).isNull();
        assertJsonEquals("{\"QUIC\":{}}", mockBuilderImpl.mEffectiveExperimentalOptions);
    }

    @Test
    @MediumTest
    public void testEnablePathDegradingConnectionMigration_justPort() {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        CronetEngine.Builder builder = new CronetEngine.Builder(mockBuilderImpl);

        builder.setConnectionMigrationOptions(
                ConnectionMigrationOptions.builder().enablePathDegradationMigration(true));
        builder.build();

        assertThat(mockBuilderImpl.mConnectionMigrationOptions).isNull();
        assertJsonEquals(
                "{\"QUIC\":{\"allow_port_migration\":true}}",
                mockBuilderImpl.mEffectiveExperimentalOptions);
    }

    @Test
    @MediumTest
    public void testEnablePathDegradingConnectionMigration_bothTrue() {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        CronetEngine.Builder builder = new CronetEngine.Builder(mockBuilderImpl);

        builder.setConnectionMigrationOptions(
                ConnectionMigrationOptions.builder()
                        .enablePathDegradationMigration(true)
                        .allowNonDefaultNetworkUsage(true));
        builder.build();

        assertThat(mockBuilderImpl.mConnectionMigrationOptions).isNull();
        assertJsonEquals(
                "{\"QUIC\":{\"migrate_sessions_early_v2\":true}}",
                mockBuilderImpl.mEffectiveExperimentalOptions);
    }

    @Test
    @MediumTest
    public void testEnablePathDegradingConnectionMigration_trueAndFalse() throws Exception {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        CronetEngine.Builder builder = new CronetEngine.Builder(mockBuilderImpl);

        builder.setConnectionMigrationOptions(
                ConnectionMigrationOptions.builder()
                        .enablePathDegradationMigration(true)
                        .allowNonDefaultNetworkUsage(false));
        builder.build();

        assertThat(mockBuilderImpl.mConnectionMigrationOptions).isNull();
        assertJsonEquals(
                "{\"QUIC\":{\"migrate_sessions_early_v2\":false,\"allow_port_migration\":true}}",
                mockBuilderImpl.mEffectiveExperimentalOptions);
    }

    @Test
    @MediumTest
    public void testEnablePathDegradingConnectionMigration_invalid() {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        CronetEngine.Builder builder = new CronetEngine.Builder(mockBuilderImpl);

        builder.setConnectionMigrationOptions(
                ConnectionMigrationOptions.builder()
                        .enablePathDegradationMigration(false)
                        .allowNonDefaultNetworkUsage(true));

        IllegalArgumentException e = assertThrows(IllegalArgumentException.class, builder::build);
        assertThat(e)
                .hasMessageThat()
                .contains(
                        "Unable to turn on non-default network usage without path degradation"
                                + " migration");
    }

    @Test
    @MediumTest
    public void testExperimentalOptions_allSet_viaExperimentalEngine() throws Exception {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        testExperimentalOptionsAllSetImpl(
                new CronetEngine.Builder(mockBuilderImpl), mockBuilderImpl);
    }

    @Test
    @MediumTest
    public void testExperimentalOptions_allSet_viaNonExperimentalEngine() throws Exception {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        testExperimentalOptionsAllSetImpl(
                new CronetEngine.Builder(mockBuilderImpl), mockBuilderImpl);
    }

    private static void testExperimentalOptionsAllSetImpl(
            CronetEngine.Builder builder, MockCronetBuilderImpl mockBuilderImpl) throws Exception {
        QuicOptions quicOptions =
                QuicOptions.builder()
                        .addAllowedQuicHost("quicHost1.com")
                        .addAllowedQuicHost("quicHost2.com")
                        .addEnabledQuicVersion("quicVersion1")
                        .addEnabledQuicVersion("quicVersion2")
                        .addEnabledQuicVersion("quicVersion1")
                        .addClientConnectionOption("clientConnectionOption1")
                        .addClientConnectionOption("clientConnectionOption2")
                        .addClientConnectionOption("clientConnectionOption1")
                        .addConnectionOption("connectionOption1")
                        .addConnectionOption("connectionOption2")
                        .addConnectionOption("connectionOption1")
                        .addExtraQuicheFlag("extraQuicheFlag1")
                        .addExtraQuicheFlag("extraQuicheFlag2")
                        .addExtraQuicheFlag("extraQuicheFlag1")
                        .setCryptoHandshakeTimeoutSeconds(toTelephoneKeyboardSequence("cryptoHs"))
                        .setIdleConnectionTimeoutSeconds(
                                toTelephoneKeyboardSequence("idleConTimeout"))
                        .setHandshakeUserAgent("handshakeUserAgent")
                        .setInitialBrokenServicePeriodSeconds(
                                toTelephoneKeyboardSequence("initialBrokenServicePeriod"))
                        .setInMemoryServerConfigsCacheSize(
                                toTelephoneKeyboardSequence("inMemoryCacheSize"))
                        .setPreCryptoHandshakeIdleTimeoutSeconds(
                                toTelephoneKeyboardSequence("preCryptoHs"))
                        .setRetransmittableOnWireTimeoutMillis(
                                toTelephoneKeyboardSequence("retransmitOnWireTo"))
                        .retryWithoutAltSvcOnQuicErrors(false)
                        .enableTlsZeroRtt(true)
                        .closeSessionsOnIpChange(false)
                        .goawaySessionsOnIpChange(true)
                        .delayJobsWithAvailableSpdySession(false)
                        .increaseBrokenServicePeriodExponentially(true)
                        .build();

        DnsOptions dnsOptions =
                DnsOptions.builder()
                        .enableStaleDns(true)
                        .preestablishConnectionsToStaleDnsResults(false)
                        .persistHostCache(true)
                        .setPersistHostCachePeriodMillis(
                                toTelephoneKeyboardSequence("persistDelay"))
                        .useBuiltInDnsResolver(false)
                        .setStaleDnsOptions(
                                StaleDnsOptions.builder()
                                        .allowCrossNetworkUsage(true)
                                        .setFreshLookupTimeoutMillis(
                                                toTelephoneKeyboardSequence("freshLookup"))
                                        .setMaxExpiredDelayMillis(
                                                toTelephoneKeyboardSequence("maxExpAge"))
                                        .useStaleOnNameNotResolved(false))
                        .build();

        ConnectionMigrationOptions connectionMigrationOptions =
                ConnectionMigrationOptions.builder()
                        .enableDefaultNetworkMigration(false)
                        .enablePathDegradationMigration(true)
                        .allowServerMigration(false)
                        .migrateIdleConnections(true)
                        .setIdleConnectionMigrationPeriodSeconds(
                                toTelephoneKeyboardSequence("idlePeriod"))
                        .retryPreHandshakeErrorsOnNonDefaultNetwork(false)
                        .allowNonDefaultNetworkUsage(true)
                        .setMaxTimeOnNonDefaultNetworkSeconds(
                                toTelephoneKeyboardSequence("maxTimeNotDefault"))
                        .setMaxWriteErrorNonDefaultNetworkMigrationsCount(
                                toTelephoneKeyboardSequence("writeErr"))
                        .setMaxPathDegradingNonDefaultNetworkMigrationsCount(
                                toTelephoneKeyboardSequence("badPathErr"))
                        .build();

        builder.setDnsOptions(dnsOptions)
                .setConnectionMigrationOptions(connectionMigrationOptions)
                .setQuicOptions(quicOptions)
                .build();

        String formattedJson =
                "{  \"AsyncDNS\": {    \"enable\": false  },  \"StaleDNS\": {    \"enable\": true, "
                        + "   \"persist_to_disk\": true,    \"persist_delay_ms\": 737740529,   "
                        + " \"allow_other_network\": true,    \"delay_ms\": 373740587,   "
                        + " \"use_stale_on_name_not_resolved\": false,    \"max_expired_time_ms\":"
                        + " 629397243  },  \"QUIC\": {    \"race_stale_dns_on_connection\": false,   "
                        + " \"migrate_sessions_on_network_change_v2\": false,   "
                        + " \"allow_server_migration\": false,    \"migrate_idle_sessions\": true,   "
                        + " \"idle_session_migration_period_seconds\": 435370463,   "
                        + " \"retry_on_alternate_network_before_handshake\": false,   "
                        + " \"max_time_on_non_default_network_seconds\": 629840858,   "
                        + " \"max_migrations_to_non_default_network_on_path_degrading\": 223720377,   "
                        + " \"max_migrations_to_non_default_network_on_write_error\": 7483377,   "
                        + " \"migrate_sessions_early_v2\": true,    \"host_whitelist\":"
                        + " \"quicHost1.com,quicHost2.com\",    \"quic_version\":"
                        + " \"quicVersion1,quicVersion2\",    \"connection_options\":"
                        + " \"connectionOption1,connectionOption2\",    \"client_connection_options\": "
                        + "        \"clientConnectionOption1,clientConnectionOption2\",   "
                        + " \"set_quic_flags\": \"extraQuicheFlag1,extraQuicheFlag2\",   "
                        + " \"max_server_configs_stored_in_properties\": 466360493,   "
                        + " \"user_agent_id\": \"handshakeUserAgent\",   "
                        + " \"retry_without_alt_svc_on_quic_errors\": false,   "
                        + " \"disable_tls_zero_rtt\": false,   "
                        + " \"max_idle_time_before_crypto_handshake_seconds\": 773270647,   "
                        + " \"max_time_before_crypto_handshake_seconds\": 27978647,   "
                        + " \"idle_connection_timeout_seconds\": 435320688,   "
                        + " \"retransmittable_on_wire_timeout_milliseconds\": 738720386,   "
                        + " \"close_sessions_on_ip_change\": false,   "
                        + " \"goaway_sessions_on_ip_change\": true,   "
                        + " \"initial_delay_for_broken_alternative_service_seconds\": 464840463,   "
                        + " \"exponential_backoff_on_initial_delay\": true,   "
                        + " \"delay_main_job_with_available_spdy_session\": false  }}";

        assertJsonEquals(formattedJson, mockBuilderImpl.mEffectiveExperimentalOptions);
    }

    @Test
    @MediumTest
    public void testExperimentalOptions_noneSet() {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        CronetEngine.Builder builder =
                new CronetEngine.Builder(mockBuilderImpl)
                        .setQuicOptions(QuicOptions.builder().build())
                        .setConnectionMigrationOptions(ConnectionMigrationOptions.builder().build())
                        .setDnsOptions(DnsOptions.builder().build());

        builder.build();
        assertJsonEquals(
                "{\"QUIC\":{},\"AsyncDNS\":{},\"StaleDNS\":{}}",
                mockBuilderImpl.mEffectiveExperimentalOptions);
    }

    private static int toTelephoneKeyboardSequence(String string) {
        int length = string.length();
        if (length > 9) {
            return toTelephoneKeyboardSequence(string.substring(0, 5)) * 10000
                    + toTelephoneKeyboardSequence(string.substring(length - 3, length));
        }

        // This could be optimized a lot but little inefficiency in tests doesn't matter all that
        // much and readability benefits are quite significant.
        Map<String, Integer> charMap = new HashMap<>();
        charMap.put("abc", 2);
        charMap.put("def", 3);
        charMap.put("ghi", 4);
        charMap.put("jkl", 5);
        charMap.put("mno", 6);
        charMap.put("pqrs", 7);
        charMap.put("tuv", 8);
        charMap.put("xyz", 9);

        int result = 0;
        for (int i = 0; i < length; i++) {
            result *= 10;
            for (Map.Entry<String, Integer> mapping : charMap.entrySet()) {
                if (mapping.getKey()
                        .contains(string.substring(i, i + 1).toLowerCase(Locale.ROOT))) {
                    result += mapping.getValue();
                    break;
                }
            }
        }
        return result;
    }

    private static void assertJsonEquals(String expected, String actual) {
        try {
            JSONObject expectedJson = new JSONObject(expected);
            JSONObject actualJson = new JSONObject(actual);

            assertJsonEquals(expectedJson, actualJson, "");
        } catch (JSONException e) {
            throw new AssertionError(e);
        }
    }

    private static void assertJsonEquals(JSONObject expected, JSONObject actual, String currentPath)
            throws JSONException {
        assertThat(jsonKeys(actual)).isEqualTo(jsonKeys(expected));

        for (String key : jsonKeys(expected)) {
            Object expectedValue = expected.get(key);
            Object actualValue = actual.get(key);
            if (expectedValue == actualValue) {
                continue;
            }
            String fullKey = currentPath.isEmpty() ? key : currentPath + "." + key;
            if (expectedValue instanceof JSONObject) {
                assertWithMessage("key is '" + fullKey + "'")
                        .that(actualValue)
                        .isInstanceOf(JSONObject.class);
                assertJsonEquals((JSONObject) expectedValue, (JSONObject) actualValue, fullKey);
            } else {
                assertWithMessage("key is '" + fullKey + "'")
                        .that(actualValue)
                        .isEqualTo(expectedValue);
            }
        }
    }

    private static Set<String> jsonKeys(JSONObject json) throws JSONException {
        Set<String> result = new HashSet<>();

        Iterator<String> keys = json.keys();

        while (keys.hasNext()) {
            String key = keys.next();
            result.add(key);
        }

        return result;
    }

    // Mocks make life downstream miserable so use a custom mock-like class.
    private static class MockCronetBuilderImpl extends ICronetEngineBuilder {
        private ConnectionMigrationOptions mConnectionMigrationOptions;
        private String mTempExperimentalOptions;
        private String mEffectiveExperimentalOptions;

        private final boolean mSupportsConnectionMigrationConfigOption;

        static MockCronetBuilderImpl withNativeSetterSupport() {
            return new MockCronetBuilderImpl(true);
        }

        static MockCronetBuilderImpl withoutNativeSetterSupport() {
            return new MockCronetBuilderImpl(false);
        }

        private MockCronetBuilderImpl(boolean supportsConnectionMigrationConfigOption) {
            this.mSupportsConnectionMigrationConfigOption = supportsConnectionMigrationConfigOption;
        }

        @Override
        public ICronetEngineBuilder addPublicKeyPins(
                String hostName,
                Set<byte[]> pinsSha256,
                boolean includeSubdomains,
                Date expirationDate) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder addQuicHint(String host, int port, int alternatePort) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder enableHttp2(boolean value) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder enableHttpCache(int cacheMode, long maxSize) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder enablePublicKeyPinningBypassForLocalTrustAnchors(
                boolean value) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder enableQuic(boolean value) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder enableSdch(boolean value) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder setExperimentalOptions(String options) {
            mTempExperimentalOptions = options;
            return this;
        }

        @Override
        public ICronetEngineBuilder setLibraryLoader(CronetEngine.Builder.LibraryLoader loader) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder setStoragePath(String value) {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder setUserAgent(String userAgent) {
            throw new UnsupportedOperationException();
        }

        @Override
        public String getDefaultUserAgent() {
            throw new UnsupportedOperationException();
        }

        @Override
        public ICronetEngineBuilder setConnectionMigrationOptions(
                ConnectionMigrationOptions options) {
            mConnectionMigrationOptions = options;
            return this;
        }

        @Override
        public Set<Integer> getSupportedConfigOptions() {
            if (mSupportsConnectionMigrationConfigOption) {
                return Collections.singleton(ICronetEngineBuilder.CONNECTION_MIGRATION_OPTIONS);
            } else {
                return Collections.emptySet();
            }
        }

        @Override
        public ExperimentalCronetEngine build() {
            mEffectiveExperimentalOptions = mTempExperimentalOptions;
            return null;
        }
    }
}
