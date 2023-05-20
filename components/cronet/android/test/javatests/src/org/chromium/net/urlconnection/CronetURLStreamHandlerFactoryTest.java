// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetTestRule;
import org.chromium.net.CronetTestRule.CronetTestFramework;

/**
 * Test for CronetURLStreamHandlerFactory.
 */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public class CronetURLStreamHandlerFactoryTest {
    @Rule
    public final CronetTestRule mTestRule = new CronetTestRule();

    private CronetTestFramework mTestFramework;

    @Before
    public void setUp() throws Exception {
        mTestFramework = mTestRule.startCronetTestFramework();
    }

    @After
    public void tearDown() throws Exception {
        mTestFramework.shutdownEngine();
    }

    @Test
    @SmallTest
    public void testRequireConfig() throws Exception {
        try {
            new CronetURLStreamHandlerFactory(null);
            fail();
        } catch (NullPointerException e) {
            assertThat(e).hasMessageThat().isEqualTo("CronetEngine is null.");
        }
    }
}
