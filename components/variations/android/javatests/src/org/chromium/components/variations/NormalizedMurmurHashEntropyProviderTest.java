// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.metrics.LowEntropySource;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/** Unit tests for {@link NormalizedMurmurHashEntropyProvider}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class NormalizedMurmurHashEntropyProviderTest {
    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @MediumTest
    public void testMurmurHashHelper() {
        // Pick some likely edge case values.
        int[] seeds = {
            -2,
            -1,
            0,
            1,
            2,
            Integer.MAX_VALUE - 2,
            Integer.MAX_VALUE - 1,
            Integer.MAX_VALUE,
            Integer.MIN_VALUE,
            Integer.MIN_VALUE + 1,
            Integer.MIN_VALUE + 2
        };

        final int max16 = 65535;
        int[] data = {
            0, max16 / 2 - 1, max16 - 2, 1, max16 / 2, max16 - 1, 2, max16 / 2 + 1, max16
        };

        for (int seed : seeds) {
            for (int datum : data) {
                int expected =
                        NormalizedMurmurHashEntropyProviderTestUtilsBridge.murmurHash16(
                                seed, datum);
                assertEquals(expected, NormalizedMurmurHashEntropyProvider.hash16(seed, datum));
            }
        }
    }

    @Test
    @MediumTest
    public void testGetEntropySource() {
        int[] randomizationSeeds = {1, 1234, 65536, Integer.MAX_VALUE, Integer.MIN_VALUE};
        int entropyRange = LowEntropySource.MAX_LOW_ENTROPY_SIZE;
        double delta = 0.00001;

        for (int seed : randomizationSeeds) {
            for (int entropyValue = 0; entropyValue < entropyRange; ++entropyValue) {
                double expected = computeEntropyNative(seed, entropyValue, entropyRange);
                double actual = computeEntropyJava(seed, entropyValue, entropyRange);
                assertEquals(expected, actual, delta);
            }
        }
    }

    private double computeEntropyNative(int randomizationSeed, int entropyValue, int entropyRange) {
        return NormalizedMurmurHashEntropyProviderTestUtilsBridge.getEntropyForTrial(
                randomizationSeed, entropyValue, entropyRange);
    }

    private double computeEntropyJava(int randomizationSeed, int entropyValue, int entropyRange) {
        NormalizedMurmurHashEntropyProvider entropyProvider =
                new NormalizedMurmurHashEntropyProvider(entropyValue, entropyRange);
        return entropyProvider.getEntropyForTrial(randomizationSeed);
    }
}
