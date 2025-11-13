// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.TruthJUnit.assume;

import static org.junit.Assert.fail;

import static org.chromium.net.ExperimentalOptionsTranslationTestUtil.assertJsonEquals;
import static org.chromium.net.impl.AndroidHttpEngineBuilderWrapper.parseConnectionMigrationOptions;
import static org.chromium.net.impl.AndroidHttpEngineBuilderWrapper.parseDnsOptions;
import static org.chromium.net.impl.AndroidHttpEngineBuilderWrapper.parseQuicOptions;

import android.content.Context;
import android.net.http.HttpEngine;
import android.os.Build;

import androidx.annotation.OptIn;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetEngine;
import org.chromium.net.ExperimentalCronetEngine;
import org.chromium.net.ExperimentalOptionsTranslationTestUtil.MockCronetBuilderImpl;
import org.chromium.net.ICronetEngineBuilder;
import org.chromium.net.telemetry.ExperimentalOptions;

import java.time.Duration;
import java.util.concurrent.atomic.AtomicBoolean;

@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
@OptIn(
        markerClass = {
            org.chromium.net.ConnectionMigrationOptions.Experimental.class,
            org.chromium.net.DnsOptions.Experimental.class,
            org.chromium.net.QuicOptions.Experimental.class
        })
public class AndroidHttpEngineBuilderWrapperTest {
    private static final String EXPECTED_EARLY_MIGRATION_ENABLED_STRING =
            "{\"QUIC\":{\"allow_port_migration\":true,\"migrate_sessions_early_v2\":true,\"migrate_"
                    + "sessions_on_network_change_v2\":true}}";

    @Before
    public void setUp() {
        assume().that(Build.VERSION.SDK_INT).isAtLeast(Build.VERSION_CODES.UPSIDE_DOWN_CAKE);
    }

    // These options have a 1:1 mapping from the jsonOption to the ConnectionMigrationOption. So
    // we are just testing that the values of ConnectionMigration are properly propagated.

    @Test
    @SmallTest
    public void testParseConnectionMigrationOptions_returnsCorrectValues() {
        ExperimentalOptions options =
                new ExperimentalOptions(
                        "{\"QUIC\":{\"migrate_sessions_on_network_change_v2\":true,"
                                + " \"allow_port_migration\":false}}");

        android.net.http.ConnectionMigrationOptions CMOptions =
                parseConnectionMigrationOptions(options);
        assertThat(CMOptions.getDefaultNetworkMigration())
                .isEqualTo(android.net.http.ConnectionMigrationOptions.MIGRATION_OPTION_ENABLED);
        assertThat(CMOptions.getPathDegradationMigration())
                .isEqualTo(android.net.http.ConnectionMigrationOptions.MIGRATION_OPTION_DISABLED);
        assertThat(CMOptions.getAllowNonDefaultNetworkUsage())
                .isEqualTo(
                        android.net.http.ConnectionMigrationOptions.MIGRATION_OPTION_UNSPECIFIED);
    }

    // -----------------------------
    // ConnectionMigrationOptions configuration does not allow enabling early_migration without
    // port_migration ie allowNonDefaultNetworkUsage requires PathDegradationMigration. So
    // setting migrate_sessions_early_v2 also turns on port migration. These tests below confirm
    // that both options are populated and being translated back to json correctly.

    @Test
    @SmallTest
    public void testSetExperimentalOption_connectionMigrationOptions_justEarlyMigration() {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        AndroidHttpEngineBuilderWrapper httpEngineBuilder =
                new AndroidHttpEngineBuilderWrapper(
                        new ApiHelper.MockHttpEngineBuilder(
                                ApplicationProvider.getApplicationContext(),
                                new CronetEngine.Builder(mockBuilderImpl)));

        httpEngineBuilder.setExperimentalOptions("{\"QUIC\":{\"migrate_sessions_early_v2\":true}}");
        httpEngineBuilder.build();

        assertThat(mockBuilderImpl.mConnectionMigrationOptions).isNull();
        assertJsonEquals(
                EXPECTED_EARLY_MIGRATION_ENABLED_STRING,
                mockBuilderImpl.mEffectiveExperimentalOptions);
    }

    @Test
    @SmallTest
    public void
            testSetExperimentalOption_connectionMigrationOptions_bothPortAndEarlyMigrationTrue() {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        AndroidHttpEngineBuilderWrapper httpEngineBuilder =
                new AndroidHttpEngineBuilderWrapper(
                        new ApiHelper.MockHttpEngineBuilder(
                                ApplicationProvider.getApplicationContext(),
                                new CronetEngine.Builder(mockBuilderImpl)));

        httpEngineBuilder.setExperimentalOptions(
                "{\"QUIC\":{\"allow_port_migration\":true, \"migrate_sessions_early_v2\":true}}");
        httpEngineBuilder.build();

        assertThat(mockBuilderImpl.mConnectionMigrationOptions).isNull();
        assertJsonEquals(
                EXPECTED_EARLY_MIGRATION_ENABLED_STRING,
                mockBuilderImpl.mEffectiveExperimentalOptions);
    }

    @Test
    @SmallTest
    public void
            testSetExperimentalOption_connectionMigrationOptions_portAndEarlyMigrationFalseTrue() {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        AndroidHttpEngineBuilderWrapper httpEngineBuilder =
                new AndroidHttpEngineBuilderWrapper(
                        new ApiHelper.MockHttpEngineBuilder(
                                ApplicationProvider.getApplicationContext(),
                                new CronetEngine.Builder(mockBuilderImpl)));

        httpEngineBuilder.setExperimentalOptions(
                "{\"QUIC\":{\"allow_port_migration\":false, \"migrate_sessions_early_v2\":true}}");
        httpEngineBuilder.build();

        assertThat(mockBuilderImpl.mConnectionMigrationOptions).isNull();
        assertJsonEquals(
                EXPECTED_EARLY_MIGRATION_ENABLED_STRING,
                mockBuilderImpl.mEffectiveExperimentalOptions);
    }

    @Test
    @SmallTest
    public void
            testSetExperimentalOption_connectionMigrationOptions_bothPortAndEarlyMigrationFalse() {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        AndroidHttpEngineBuilderWrapper httpEngineBuilder =
                new AndroidHttpEngineBuilderWrapper(
                        new ApiHelper.MockHttpEngineBuilder(
                                ApplicationProvider.getApplicationContext(),
                                new CronetEngine.Builder(mockBuilderImpl)));

        httpEngineBuilder.setExperimentalOptions(
                "{\"QUIC\":{\"allow_port_migration\":false, \"migrate_sessions_early_v2\":false}}");
        httpEngineBuilder.build();

        assertThat(mockBuilderImpl.mConnectionMigrationOptions).isNull();
        assertJsonEquals(
                "{\"QUIC\":{\"allow_port_migration\":false, \"migrate_sessions_early_v2\":false}}",
                mockBuilderImpl.mEffectiveExperimentalOptions);
    }

    @Test
    @SmallTest
    public void
            testSetExperimentalOption_connectionMigrationOptions_portAndEarlyMigrationTrueFalse() {
        MockCronetBuilderImpl mockBuilderImpl = MockCronetBuilderImpl.withoutNativeSetterSupport();
        AndroidHttpEngineBuilderWrapper httpEngineBuilder =
                new AndroidHttpEngineBuilderWrapper(
                        new ApiHelper.MockHttpEngineBuilder(
                                ApplicationProvider.getApplicationContext(),
                                new CronetEngine.Builder(mockBuilderImpl)));

        httpEngineBuilder.setExperimentalOptions(
                "{\"QUIC\":{\"allow_port_migration\":true, \"migrate_sessions_early_v2\":false}}");
        httpEngineBuilder.build();

        assertThat(mockBuilderImpl.mConnectionMigrationOptions).isNull();
        assertJsonEquals(
                "{\"QUIC\":{\"allow_port_migration\":true, \"migrate_sessions_early_v2\":false}}",
                mockBuilderImpl.mEffectiveExperimentalOptions);
    }

    // ------------ End migrate_sessions_early_v2 specific tests -------------------

    @Test
    @SmallTest
    public void testParseDnsOptions_allSet_returnsCorrectValues() {
        long delay_ms = 373740587;
        long persist_delay_ms = 737740529;
        long max_expired_time_ms = 629397243;
        ExperimentalOptions options =
                new ExperimentalOptions(
                        "{  \"AsyncDNS\": { \"enable\": true },  \"StaleDNS\": {    \"enable\":"
                                + " true,  \"persist_to_disk\": false,    \"persist_delay_ms\": "
                                + persist_delay_ms
                                + ",\"allow_other_network\": true,    \"delay_ms\": "
                                + delay_ms
                                + ",\"use_stale_on_name_not_resolved\": true,"
                                + " \"max_expired_time_ms\":"
                                + max_expired_time_ms
                                + "  },  \"QUIC\": {    \"race_stale_dns_on_connection\": true }}");

        android.net.http.DnsOptions dnsOptions = parseDnsOptions(options);
        // AsyncDNS
        assertThat(dnsOptions.getUseHttpStackDnsResolver())
                .isEqualTo(android.net.http.DnsOptions.DNS_OPTION_ENABLED);
        // persist_to_disk
        assertThat(dnsOptions.getPersistHostCache())
                .isEqualTo(android.net.http.DnsOptions.DNS_OPTION_DISABLED);
        assertThat(dnsOptions.getPersistHostCachePeriod())
                .isEqualTo(Duration.ofMillis(persist_delay_ms));
        assertThat(dnsOptions.getStaleDns())
                .isEqualTo(android.net.http.DnsOptions.DNS_OPTION_ENABLED);
        // race_stale_dns_on_connection
        assertThat(dnsOptions.getPreestablishConnectionsToStaleDnsResults())
                .isEqualTo(android.net.http.DnsOptions.DNS_OPTION_ENABLED);

        android.net.http.DnsOptions.StaleDnsOptions staleDnsOptions =
                dnsOptions.getStaleDnsOptions();
        assertThat(staleDnsOptions.getFreshLookupTimeout()).isEqualTo(Duration.ofMillis(delay_ms));
        assertThat(staleDnsOptions.getMaxExpiredDelay())
                .isEqualTo(Duration.ofMillis(max_expired_time_ms));
        // allow_other_network
        assertThat(staleDnsOptions.getAllowCrossNetworkUsage())
                .isEqualTo(android.net.http.DnsOptions.DNS_OPTION_ENABLED);
        assertThat(staleDnsOptions.getUseStaleOnNameNotResolved())
                .isEqualTo(android.net.http.DnsOptions.DNS_OPTION_ENABLED);
    }

    @Test
    @SmallTest
    public void testParseDnsOptions_noneSet_returnsCorrectValues() {
        ExperimentalOptions options =
                new ExperimentalOptions(
                        "{  \"AsyncDNS\": { },  \"StaleDNS\": { },  \"QUIC\": { }}");

        android.net.http.DnsOptions dnsOptions = parseDnsOptions(options);
        // AsyncDNS
        assertThat(dnsOptions.getUseHttpStackDnsResolver())
                .isEqualTo(android.net.http.DnsOptions.DNS_OPTION_UNSPECIFIED);
        // persist_to_disk
        assertThat(dnsOptions.getPersistHostCache())
                .isEqualTo(android.net.http.DnsOptions.DNS_OPTION_UNSPECIFIED);
        assertThat(dnsOptions.getPersistHostCachePeriod()).isNull();
        assertThat(dnsOptions.getStaleDns())
                .isEqualTo(android.net.http.DnsOptions.DNS_OPTION_UNSPECIFIED);
        // race_stale_dns_on_connection
        assertThat(dnsOptions.getPreestablishConnectionsToStaleDnsResults())
                .isEqualTo(android.net.http.DnsOptions.DNS_OPTION_UNSPECIFIED);

        android.net.http.DnsOptions.StaleDnsOptions staleDnsOptions =
                dnsOptions.getStaleDnsOptions();
        assertThat(staleDnsOptions.getFreshLookupTimeout()).isNull();
        assertThat(staleDnsOptions.getMaxExpiredDelay()).isNull();
        // allow_other_network
        assertThat(staleDnsOptions.getAllowCrossNetworkUsage())
                .isEqualTo(android.net.http.DnsOptions.DNS_OPTION_UNSPECIFIED);
        assertThat(staleDnsOptions.getUseStaleOnNameNotResolved())
                .isEqualTo(android.net.http.DnsOptions.DNS_OPTION_UNSPECIFIED);
    }

    @Test
    @SmallTest
    public void testParseQuicOptions_allSet_returnsCorrectValues() {
        int max_server_config = 466360493;
        int idle_conn_timeout = 435320688;
        String user_agent_id = "handshakeUserAgent";
        String host_whitelist = "quicHost1.com,quicHost2.com";
        ExperimentalOptions options =
                new ExperimentalOptions(
                        "{  \"QUIC\": {   \"host_whitelist\": \""
                                + host_whitelist
                                + "\",   \"max_server_configs_stored_in_properties\": "
                                + max_server_config
                                + ",  \"user_agent_id\": \""
                                + user_agent_id
                                + "\",   \"idle_connection_timeout_seconds\": "
                                + idle_conn_timeout
                                + "   }}");
        android.net.http.QuicOptions quicOptions = parseQuicOptions(options);

        assertThat(quicOptions.getAllowedQuicHosts())
                .containsExactlyElementsIn(host_whitelist.split(","));
        assertThat(quicOptions.getInMemoryServerConfigsCacheSize()).isEqualTo(max_server_config);
        assertThat(quicOptions.getHandshakeUserAgent()).isEqualTo(user_agent_id);
        assertThat(quicOptions.getIdleConnectionTimeout())
                .isEqualTo(Duration.ofSeconds(idle_conn_timeout));
    }

    @Test
    @SmallTest
    public void testParseQuicOptions_noneSet_returnsCorrectValues() {
        ExperimentalOptions options = new ExperimentalOptions("{  \"QUIC\": {  }}");
        android.net.http.QuicOptions quicOptions = parseQuicOptions(options);

        assertThat(quicOptions.getAllowedQuicHosts()).isEmpty();
        assertThat(quicOptions.hasInMemoryServerConfigsCacheSize()).isFalse();
        assertThat(quicOptions.getHandshakeUserAgent()).isNull();
        assertThat(quicOptions.getIdleConnectionTimeout()).isNull();
    }

    @Test
    @SmallTest
    public void testConnectionMigrationOptions_someSet_setExperimentalOptionsIsCalled() {
        AtomicBoolean setExperimentalOptionsCalled = new AtomicBoolean(false);
        // Because set*Options is not directly implemented in the wrapper, this test serves as a
        // guard to prevent breaking this feature if we change how set*Options is implemented.
        CronetEngine.Builder builder =
                new CronetEngine.Builder(
                        new AndroidHttpEngineBuilderWrapper(null) {
                            @Override
                            public ICronetEngineBuilder setExperimentalOptions(String options) {
                                setExperimentalOptionsCalled.set(true);
                                return this;
                            }

                            @Override
                            public ExperimentalCronetEngine build() {
                                return null;
                            }
                        });
        org.chromium.net.ConnectionMigrationOptions.Builder optionsBuilder =
                org.chromium.net.ConnectionMigrationOptions.builder();
        optionsBuilder.allowNonDefaultNetworkUsage(true);
        optionsBuilder.allowServerMigration(true);
        optionsBuilder.enablePathDegradationMigration(true);
        builder.setConnectionMigrationOptions(optionsBuilder.build());
        builder.build();

        assertThat(setExperimentalOptionsCalled.get()).isTrue();
    }

    @Test
    @SmallTest
    public void testDnsOptions_someSet_setExperimentalOptionsIsCalled() {
        AtomicBoolean setExperimentalOptionsCalled = new AtomicBoolean(false);
        // Because set*Options is not directly implemented in the wrapper, this test serves as a
        // guard to prevent breaking this feature if we change how set*Options is implemented.
        CronetEngine.Builder builder =
                new CronetEngine.Builder(
                        new AndroidHttpEngineBuilderWrapper(null) {
                            @Override
                            public ICronetEngineBuilder setExperimentalOptions(String options) {
                                setExperimentalOptionsCalled.set(true);
                                return this;
                            }

                            @Override
                            public ExperimentalCronetEngine build() {
                                return null;
                            }
                        });
        org.chromium.net.DnsOptions.Builder optionsBuilder = org.chromium.net.DnsOptions.builder();
        optionsBuilder.enableStaleDns(true);
        optionsBuilder.persistHostCache(true);
        optionsBuilder.useBuiltInDnsResolver(true);
        optionsBuilder.preestablishConnectionsToStaleDnsResults(true);
        optionsBuilder.setStaleDnsOptions(org.chromium.net.DnsOptions.StaleDnsOptions.builder());
        builder.setDnsOptions(optionsBuilder.build());
        builder.build();

        assertThat(setExperimentalOptionsCalled.get()).isTrue();
    }

    @Test
    @SmallTest
    public void testQuicOptions_someSet_setExperimentalOptionsIsCalled() {
        AtomicBoolean setExperimentalOptionsCalled = new AtomicBoolean(false);
        // Because set*Options is not directly implemented in the wrapper, this test serves as a
        // guard to prevent breaking this feature if we change how set*Options is implemented.
        CronetEngine.Builder builder =
                new CronetEngine.Builder(
                        new AndroidHttpEngineBuilderWrapper(null) {
                            @Override
                            public ICronetEngineBuilder setExperimentalOptions(String options) {
                                setExperimentalOptionsCalled.set(true);
                                return this;
                            }

                            @Override
                            public ExperimentalCronetEngine build() {
                                return null;
                            }
                        });
        org.chromium.net.QuicOptions.Builder optionsBuilder =
                org.chromium.net.QuicOptions.builder();
        optionsBuilder.addAllowedQuicHost("test.com");
        optionsBuilder.setInMemoryServerConfigsCacheSize(1);
        optionsBuilder.setHandshakeUserAgent("test");
        optionsBuilder.setIdleConnectionTimeout(Duration.ZERO);
        builder.setQuicOptions(optionsBuilder.build());
        builder.build();

        assertThat(setExperimentalOptionsCalled.get()).isTrue();
    }

    @Test
    @SmallTest
    public void testOptions_noneSet_setExperimentalOptionsNotCalled() {
        // Because set*Options is not directly implemented in the wrapper, this test serves as a
        // guard to prevent breaking this feature if we change how set*Options is implemented.
        CronetEngine.Builder builder =
                new CronetEngine.Builder(
                        new AndroidHttpEngineBuilderWrapper(null) {
                            @Override
                            public ICronetEngineBuilder setExperimentalOptions(String options) {
                                fail();
                                return this;
                            }

                            @Override
                            public ExperimentalCronetEngine build() {
                                return null;
                            }
                        });
        builder.build();
    }

    /**
     * JUnit uses reflection to fetch the TestClass's annotation and parameter types. Hence fails
     * when it can't find android.net.http.* class for Android T- devices. This class abstracts the
     * U+ methods away from JUnit, allowing us to compile.
     */
    private static class ApiHelper {
        public static class MockHttpEngineBuilder extends HttpEngine.Builder {
            private final CronetEngine.Builder mBackend;

            public MockHttpEngineBuilder(Context context, CronetEngine.Builder backend) {
                super(context);
                mBackend = backend;
            }

            @Override
            public HttpEngine.Builder setConnectionMigrationOptions(
                    android.net.http.ConnectionMigrationOptions options) {
                org.chromium.net.ConnectionMigrationOptions.Builder optionsBuilder =
                        org.chromium.net.ConnectionMigrationOptions.builder();

                Boolean pathDegradationValue =
                        stateToBoolean(options.getPathDegradationMigration());
                if (pathDegradationValue != null) {
                    optionsBuilder.enablePathDegradationMigration(pathDegradationValue);
                }

                Boolean allowNonDefaultNetworkUsage =
                        stateToBoolean(options.getAllowNonDefaultNetworkUsage());
                if (allowNonDefaultNetworkUsage != null) {
                    optionsBuilder.allowNonDefaultNetworkUsage(allowNonDefaultNetworkUsage);
                }

                mBackend.setConnectionMigrationOptions(optionsBuilder.build());
                return this;
            }

            @Override
            public HttpEngine build() {
                mBackend.build();
                return null;
            }

            private static Boolean stateToBoolean(int optionState) {
                switch (optionState) {
                    case android.net.http.ConnectionMigrationOptions.MIGRATION_OPTION_ENABLED:
                        return true;
                    case android.net.http.ConnectionMigrationOptions.MIGRATION_OPTION_DISABLED:
                        return false;
                    case android.net.http.ConnectionMigrationOptions.MIGRATION_OPTION_UNSPECIFIED:
                        return null;
                    default:
                        throw new AssertionError("Unknown state option");
                }
            }
        }
    }
}
