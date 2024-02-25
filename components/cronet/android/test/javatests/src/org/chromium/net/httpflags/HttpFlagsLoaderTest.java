// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.httpflags;

import static com.google.common.truth.Truth.assertThat;

import android.os.Bundle;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CronetImplementation;
import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.IgnoreFor;
import org.chromium.net.impl.CronetManifest;
import org.chromium.net.impl.CronetManifestInterceptor;

/** Tests {@link HttpFlagsLoader} */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
@IgnoreFor(
        implementations = {CronetImplementation.FALLBACK},
        reason = "These tests don't depend on Cronet's impl")
public final class HttpFlagsLoaderTest {
    @Rule public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    public CronetTestFramework mCronetTestFramework;

    @Before
    public void setUp() {
        mCronetTestFramework = mTestRule.getTestFramework();
    }

    private void setShouldReadHttpFlagsInManifest(boolean value) {
        Bundle metaData = new Bundle();
        metaData.putBoolean(CronetManifest.READ_HTTP_FLAGS_META_DATA_KEY, value);
        mCronetTestFramework.interceptContext(new CronetManifestInterceptor(metaData));
    }

    @Test
    @SmallTest
    public void testLoad_returnsNullIfNoFlags() {
        setShouldReadHttpFlagsInManifest(true);
        mCronetTestFramework.setHttpFlags(null);
        assertThat(HttpFlagsLoader.load(mCronetTestFramework.getContext())).isNull();
    }

    @Test
    @SmallTest
    public void testLoad_returnsFileFlagContents() {
        setShouldReadHttpFlagsInManifest(true);
        Flags flags =
                Flags.newBuilder()
                        .putFlags(
                                "test_flag_name",
                                FlagValue.newBuilder()
                                        .addConstrainedValues(
                                                FlagValue.ConstrainedValue.newBuilder()
                                                        .setStringValue("test_flag_value")
                                                        .build())
                                        .build())
                        .build();
        mCronetTestFramework.setHttpFlags(flags);
        assertThat(HttpFlagsLoader.load(mCronetTestFramework.getContext())).isEqualTo(flags);
    }
}
