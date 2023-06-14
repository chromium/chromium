// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.impl.CronetLogger.CronetSource;

/**
 * Tests {@link CronetManifest}
 */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
@OnlyRunNativeCronet
public class CronetManifestTest {
    @Rule
    public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private Context mMockContext;
    private Bundle mMetadata;
    private ApplicationInfo mAppInfo;

    @Before
    public void setUp() throws Exception {
        mAppInfo = new ApplicationInfo();
        mMockContext = new MockContext(mTestRule.getTestFramework().getContext());
        mMetadata = new Bundle();
    }

    @Test
    @SmallTest
    public void testTelemetryOptIn_whenNoMetadata() throws Exception {
        assertThat(CronetManifest.isAppOptedInForTelemetry(
                           mMockContext, CronetSource.CRONET_SOURCE_STATICALLY_LINKED))
                .isFalse();
        assertThat(CronetManifest.isAppOptedInForTelemetry(
                           mMockContext, CronetSource.CRONET_SOURCE_PLAY_SERVICES))
                .isFalse();
        assertThat(CronetManifest.isAppOptedInForTelemetry(
                           mMockContext, CronetSource.CRONET_SOURCE_FALLBACK))
                .isFalse();
    }

    @Test
    @SmallTest
    public void testTelemetryOptIn_whenMetadataIsTrue() throws Exception {
        mMetadata.putBoolean(CronetManifest.TELEMETRY_OPT_IN_META_DATA_STR, true);
        mAppInfo.metaData = mMetadata;

        assertThat(CronetManifest.isAppOptedInForTelemetry(
                           mMockContext, CronetSource.CRONET_SOURCE_STATICALLY_LINKED))
                .isTrue();
        assertThat(CronetManifest.isAppOptedInForTelemetry(
                           mMockContext, CronetSource.CRONET_SOURCE_PLAY_SERVICES))
                .isTrue();
        assertThat(CronetManifest.isAppOptedInForTelemetry(
                           mMockContext, CronetSource.CRONET_SOURCE_FALLBACK))
                .isTrue();
    }

    @Test
    @SmallTest
    public void testTelemetryOptIn_whenMetadataIsFalse() throws Exception {
        mMetadata.putBoolean(CronetManifest.TELEMETRY_OPT_IN_META_DATA_STR, false);
        mAppInfo.metaData = mMetadata;

        assertThat(CronetManifest.isAppOptedInForTelemetry(
                           mMockContext, CronetSource.CRONET_SOURCE_STATICALLY_LINKED))
                .isFalse();
        assertThat(CronetManifest.isAppOptedInForTelemetry(
                           mMockContext, CronetSource.CRONET_SOURCE_PLAY_SERVICES))
                .isFalse();
        assertThat(CronetManifest.isAppOptedInForTelemetry(
                           mMockContext, CronetSource.CRONET_SOURCE_FALLBACK))
                .isFalse();
    }

    private class MockContext extends ContextWrapper {
        public MockContext(Context base) {
            super(base);
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(super.getPackageManager()) {
                @Override
                public ApplicationInfo getApplicationInfo(String packageName, int flags)
                        throws PackageManager.NameNotFoundException {
                    return mAppInfo;
                }
            };
        }
    }
}
