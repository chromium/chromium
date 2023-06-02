// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.urlconnection;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;

/**
 * Test for CronetURLStreamHandlerFactory.
 */
@Batch(Batch.UNIT_TESTS)
@RunWith(AndroidJUnit4.class)
public class CronetURLStreamHandlerFactoryTest {
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
