// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.telemetry;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;

@RunWith(AndroidJUnit4.class)
@Batch(Batch.UNIT_TESTS)
public final class RateLimiterTest {
    @Test
    public void testImmediateRateLimit() {
        RateLimiter rateLimiter = new RateLimiter(1);
        assertTrue("First request was rate limited", rateLimiter.tryAcquire());
        assertFalse("Second request was not rate limited", rateLimiter.tryAcquire());
    }

    // TODO(b/309098875): the internal repo these tests were originally written in had the following
    // tests:
    // - testOneSamplePerSecond
    // - testNoSampleSentPerSecond
    // - testMultipleSamplesPerSecond
    // Problem is these tests used the Roboelectric ShadowSystemClock, which we can't easily use
    // here. We should find a way to work around that. In the mean time, these tests are omitted.

    @Test
    public void testInvalidSamplePerSecond() {
        int samplesPerSecond = -1;
        assertThrows(
                "samples per second was negative",
                IllegalArgumentException.class,
                () -> new RateLimiter(samplesPerSecond));
    }
}
