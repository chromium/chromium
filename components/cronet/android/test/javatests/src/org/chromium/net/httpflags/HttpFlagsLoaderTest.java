
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.httpflags;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CronetTestFramework;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;

/**
 * Tests {@link HttpFlagsLoader}
 */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public final class HttpFlagsLoaderTest {
    @Rule
    public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    public CronetTestFramework mCronetTestFramework;
    @Before
    public void setUp() {
        mCronetTestFramework = mTestRule.getTestFramework();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testLoad_returnsNullIfNoFlags() {
        mCronetTestFramework.setHttpFlags(null);
        assertThat(HttpFlagsLoader.load(mCronetTestFramework.getContext())).isNull();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testLoad_returnsFileFlagContents() {
        Flags flags = Flags.newBuilder()
                              .putFlags("test_flag_name",
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
