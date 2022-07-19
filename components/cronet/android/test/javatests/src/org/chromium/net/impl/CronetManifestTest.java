// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Answers.RETURNS_DEFAULTS;
import static org.mockito.Mockito.doReturn;

import static org.chromium.net.CronetTestRule.getContext;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.test.runner.AndroidJUnit4;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Feature;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.impl.CronetLogger.CronetSource;

/**
 * Tests {@link CronetManifest}
 */
@RunWith(AndroidJUnit4.class)
public class CronetManifestTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mMockContext;
    @Mock(answer = RETURNS_DEFAULTS)
    private PackageManager mMockPackageManager;
    private Bundle mMetadata;
    private ApplicationInfo mAppInfo;

    @Before
    public void setUp() throws Exception {
        mMockContext = new MockContext(getContext());
        mMetadata = new Bundle();
        mAppInfo = new ApplicationInfo();
        doReturn(mAppInfo)
                .when(mMockPackageManager)
                .getApplicationInfo(mMockContext.getPackageName(), PackageManager.GET_META_DATA);
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    @Feature({"Cronet"})
    public void testTelemetryOptIn_whenNoMetadata() throws Exception {
        assertFalse(CronetManifest.isAppOptedInForTelemetry(
                mMockContext, CronetSource.CRONET_SOURCE_STATICALLY_LINKED));
        assertFalse(CronetManifest.isAppOptedInForTelemetry(
                mMockContext, CronetSource.CRONET_SOURCE_PLAY_SERVICES));
        assertFalse(CronetManifest.isAppOptedInForTelemetry(
                mMockContext, CronetSource.CRONET_SOURCE_FALLBACK));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    @Feature({"Cronet"})
    public void testTelemetryOptIn_whenMetadataIsTrue() throws Exception {
        mMetadata.putBoolean(CronetManifest.METRICS_OPT_IN_META_DATA_STR, true);
        mAppInfo.metaData = mMetadata;

        assertTrue(CronetManifest.isAppOptedInForTelemetry(
                mMockContext, CronetSource.CRONET_SOURCE_STATICALLY_LINKED));
        assertTrue(CronetManifest.isAppOptedInForTelemetry(
                mMockContext, CronetSource.CRONET_SOURCE_PLAY_SERVICES));
        assertTrue(CronetManifest.isAppOptedInForTelemetry(
                mMockContext, CronetSource.CRONET_SOURCE_FALLBACK));
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    @Feature({"Cronet"})
    public void testTelemetryOptIn_whenMetadataIsFalse() throws Exception {
        mMetadata.putBoolean(CronetManifest.METRICS_OPT_IN_META_DATA_STR, false);
        mAppInfo.metaData = mMetadata;

        assertFalse(CronetManifest.isAppOptedInForTelemetry(
                mMockContext, CronetSource.CRONET_SOURCE_STATICALLY_LINKED));
        assertFalse(CronetManifest.isAppOptedInForTelemetry(
                mMockContext, CronetSource.CRONET_SOURCE_PLAY_SERVICES));
        assertFalse(CronetManifest.isAppOptedInForTelemetry(
                mMockContext, CronetSource.CRONET_SOURCE_FALLBACK));
    }

    private class MockContext extends ContextWrapper {
        public MockContext(Context base) {
            super(base);
        }

        @Override
        public PackageManager getPackageManager() {
            return mMockPackageManager;
        }
    }
}
