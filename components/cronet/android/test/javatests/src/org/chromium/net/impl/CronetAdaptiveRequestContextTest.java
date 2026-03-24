// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.impl;

import static org.junit.Assert.assertEquals;
import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.os.Build;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;

/** Test functionality of CronetAdaptiveRequestContext. */
@Batch(Batch.PER_CLASS)
@RunWith(AndroidJUnit4.class)
public class CronetAdaptiveRequestContextTest {
    private CronetAdaptiveRequestContext mContext;
    private FakeClock mFakeClock;

    private static class FakeClock extends CronetAdaptiveRequestContext.Clock {
        private long mElapsedRealtime;

        @Override
        long elapsedRealtime() {
            return mElapsedRealtime;
        }

        void advanceTime(long millis) {
            mElapsedRealtime += millis;
        }
    }

    @Before
    public void setUp() throws Exception {
        Context context = ApplicationProvider.getApplicationContext();
        mFakeClock = new FakeClock();
        mContext = new CronetAdaptiveRequestContext(context, mFakeClock);
    }

    @Test
    @SmallTest
    public void reportFallbackUsed_memorizesNetwork() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        String url = "https://example.com/path";
        Long networkHandle = 12345L;

        mContext.reportFallbackUsed(url, networkHandle);

        assertEquals(networkHandle, mContext.getFallbackNetworkHandle(url));
        assertEquals(networkHandle, mContext.getFallbackNetworkHandle("https://example.com/other"));
    }

    @Test
    @SmallTest
    public void getFallbackNetwork_expired_returnsNull() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        String url = "https://example.com/path";
        Long networkHandle = 12345L;

        mContext.reportFallbackUsed(url, networkHandle);
        assertEquals(networkHandle, mContext.getFallbackNetworkHandle(url));

        // Advance time just past the 10s expiration.
        mFakeClock.advanceTime(10001);

        assertEquals(null, mContext.getFallbackNetworkHandle(url));
    }

    @Test
    @SmallTest
    public void getFallbackNetwork_notExpired_returnsNetwork() {
        // We need java.util.stream.Stream to be available for these tests.
        assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);
        String url = "https://example.com/path";
        Long networkHandle = 12345L;

        mContext.reportFallbackUsed(url, networkHandle);

        // Advance time almost to expiration.
        mFakeClock.advanceTime(9999);

        assertEquals(networkHandle, mContext.getFallbackNetworkHandle(url));
    }
}
