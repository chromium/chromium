// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.TruthJUnit.assume;

import static org.chromium.net.ExperimentalOptionsTranslationTestUtil.assertJsonEquals;
import static org.chromium.net.impl.AndroidHttpEngineBuilderWrapper.parseConnectionMigrationOptions;

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
import org.chromium.net.ExperimentalOptionsTranslationTestUtil.MockCronetBuilderImpl;
import org.chromium.net.telemetry.ExperimentalOptions;

@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
@OptIn(markerClass = {org.chromium.net.ConnectionMigrationOptions.Experimental.class})
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
    // that
    // both options are populated and being translated back to json correctly.

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
