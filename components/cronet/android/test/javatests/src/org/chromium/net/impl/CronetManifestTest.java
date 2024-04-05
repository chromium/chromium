// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.os.Bundle;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.impl.CronetLogger.CronetSource;

/** Tests {@link CronetManifest} */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public class CronetManifestTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    public CronetTestFramework mCronetTestFramework;

    @Before
    public void setUp() {
        mCronetTestFramework = mTestRule.getTestFramework();
    }

    private void setTelemetryOptIn(boolean value) {
        Bundle metaData = new Bundle();
        metaData.putBoolean(CronetManifest.ENABLE_TELEMETRY_META_DATA_KEY, value);
        mCronetTestFramework.interceptContext(new CronetManifestInterceptor(metaData));
    }

    @Test
    @SmallTest
    public void testTelemetryOptIn_whenNoMetadata() throws Exception {
        // simulates an empty metadata file to avoid clashes with other manifest files added
        // to test binary.
        mCronetTestFramework.interceptContext(new CronetManifestInterceptor(new Bundle()));
        for (CronetSource source : CronetSource.values()) {
            switch (source) {
                case CRONET_SOURCE_STATICALLY_LINKED:
                    assertWithMessage("Check failed for " + source)
                            .that(
                                    CronetManifest.isAppOptedInForTelemetry(
                                            mCronetTestFramework.getContext(), source))
                            .isFalse();
                    break;
                case CRONET_SOURCE_PLATFORM:
                    assertWithMessage("Check failed for " + source)
                            .that(
                                    CronetManifest.isAppOptedInForTelemetry(
                                            mCronetTestFramework.getContext(), source))
                            .isTrue();
                    break;
                case CRONET_SOURCE_PLAY_SERVICES:
                    assertWithMessage("Check failed for " + source)
                            .that(
                                    CronetManifest.isAppOptedInForTelemetry(
                                            mCronetTestFramework.getContext(), source))
                            .isTrue();
                    break;
                case CRONET_SOURCE_FALLBACK:
                    assertWithMessage("Check failed for " + source)
                            .that(
                                    CronetManifest.isAppOptedInForTelemetry(
                                            mCronetTestFramework.getContext(), source))
                            .isFalse();
                    break;
                case CRONET_SOURCE_FAKE:
                    assertWithMessage("Check failed for " + source)
                            .that(
                                    CronetManifest.isAppOptedInForTelemetry(
                                            mCronetTestFramework.getContext(), source))
                            .isFalse();
                    break;
                case CRONET_SOURCE_UNSPECIFIED:
                    // This shouldn't happen, but for safety check that it will be disabled.
                    assertWithMessage("Check failed for " + source)
                            .that(
                                    CronetManifest.isAppOptedInForTelemetry(
                                            mCronetTestFramework.getContext(), source))
                            .isFalse();
                    break;
            }
        }
    }

    @Test
    @SmallTest
    public void testTelemetryOptIn_whenMetadataIsTrue() throws Exception {
        setTelemetryOptIn(true);
        for (CronetSource source : CronetSource.values()) {
            assertWithMessage("Check failed for " + source)
                    .that(
                            CronetManifest.isAppOptedInForTelemetry(
                                    mCronetTestFramework.getContext(), source))
                    .isTrue();
        }
    }

    @Test
    @SmallTest
    public void testTelemetryOptIn_whenMetadataIsFalse() throws Exception {
        setTelemetryOptIn(false);
        for (CronetSource source : CronetSource.values()) {
            assertWithMessage("Check failed for " + source)
                    .that(
                            CronetManifest.isAppOptedInForTelemetry(
                                    mCronetTestFramework.getContext(), source))
                    .isFalse();
        }
    }

    private void setReadHttpFlags(boolean value) {
        Bundle metaData = new Bundle();
        metaData.putBoolean(CronetManifest.READ_HTTP_FLAGS_META_DATA_KEY, value);
        mCronetTestFramework.interceptContext(new CronetManifestInterceptor(metaData));
    }

    @Test
    @SmallTest
    public void testShouldReadHttpFlags_whenNoMetadata() throws Exception {
        assertThat(CronetManifest.shouldReadHttpFlags(mCronetTestFramework.getContext())).isTrue();
    }

    @Test
    @SmallTest
    public void testShouldReadHttpFlags_whenMetadataIsTrue() throws Exception {
        setReadHttpFlags(true);
        assertThat(CronetManifest.shouldReadHttpFlags(mCronetTestFramework.getContext())).isTrue();
    }

    @Test
    @SmallTest
    public void testShouldReadHttpFlags_whenMetadataIsFalse() throws Exception {
        setReadHttpFlags(false);
        assertThat(CronetManifest.shouldReadHttpFlags(mCronetTestFramework.getContext())).isFalse();
    }
}
