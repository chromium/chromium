// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.impl.ImplVersion;

/**
 * Test functionality of {@link FakeCronetProvider}.
 */
@RunWith(AndroidJUnit4.class)
public class FakeCronetProviderTest {
    Context mContext;
    FakeCronetProvider mProvider;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        mProvider = new FakeCronetProvider(mContext);
    }

    @Test
    @SmallTest
    public void testGetName() {
        String expectedName = "Fake-Cronet-Provider";
        assertEquals(expectedName, mProvider.getName());
    }

    @Test
    @SmallTest
    public void testGetVersion() {
        assertEquals(ImplVersion.getCronetVersion(), mProvider.getVersion());
    }

    @Test
    @SmallTest
    public void testIsEnabled() {
        assertTrue(mProvider.isEnabled());
    }

    @Test
    @SmallTest
    public void testHashCode() {
        FakeCronetProvider otherProvider = new FakeCronetProvider(mContext);
        assertEquals(otherProvider.hashCode(), mProvider.hashCode());
    }

    @Test
    @SmallTest
    public void testEquals() {
        assertTrue(mProvider.equals(new FakeCronetProvider(mContext)));
    }
}
