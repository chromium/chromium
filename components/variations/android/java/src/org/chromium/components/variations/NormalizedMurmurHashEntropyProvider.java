// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations;

import androidx.annotation.VisibleForTesting;

/**
 * NormalizedMurmurHashEntropyProvider is an entropy provider suitable for low entropy sources
 * (below 16 bits). Java version is a re-implementation of NormalizedMurmurHashEntropyProvider
 * available in C++. One notable difference is that the Java version doesn't support trial name
 * hashing, so each trial must have a randomization seed. The implementation in this class should be
 * kept consistent with the C++ implementation in entropy_provider.h and variations_murmur_hash.h.
 */
public final class NormalizedMurmurHashEntropyProvider {
    private final int mEntropyValue;
    private final int mEntropyRange;

    /** Initializes the entropy provider with the value and the range of the low entropy source. */
    public NormalizedMurmurHashEntropyProvider(int entropyValue, int entropyRange) {
        mEntropyValue = entropyValue;
        mEntropyRange = entropyRange;
    }

    /**
     * Returns a double in the range of [0, 1) to be used for the dice roll for the specified field
     * trial. A given instance always return the same value given the same input.
     *
     * @param studyRandomizationSeed The non-zero randomization seed to be used for the study.
     */
    public double getEntropyForTrial(int studyRandomizationSeed) {
        assert studyRandomizationSeed != 0;
        int x = hash16(studyRandomizationSeed, mEntropyValue);
        int xOrdinal = 0;
        for (int i = 0; i < mEntropyRange; i++) {
            int y = hash16(studyRandomizationSeed, i);
            xOrdinal += lessUnsigned(y, x) ? 1 : 0;
        }
        assert xOrdinal < mEntropyRange;
        return ((double) xOrdinal) / mEntropyRange;
    }

    // After the minimal SDK level is bumped to 26, this should be replaced with
    // Integer.compareUnsigned.
    private static boolean lessUnsigned(int a, int b) {
        return (a + Integer.MIN_VALUE) < (b + Integer.MIN_VALUE);
    }

    @VisibleForTesting
    public static int hash16(int seed, int data) {
        int h1 = seed;
        int k1 = data;

        // tail
        k1 *= 0xcc9e2d51;
        k1 = Integer.rotateLeft(k1, 15);
        k1 *= 0x1b873593;
        h1 ^= k1;

        // finalization
        h1 ^= 2;
        h1 = finalMix(h1);

        return h1;
    }

    private static int finalMix(int h) {
        h ^= h >>> 16;
        h *= 0x85ebca6b;
        h ^= h >>> 13;
        h *= 0xc2b2ae35;
        h ^= h >>> 16;
        return h;
    }
}
