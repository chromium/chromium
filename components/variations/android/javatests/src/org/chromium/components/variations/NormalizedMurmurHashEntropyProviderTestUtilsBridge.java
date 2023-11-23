// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations;

import org.jni_zero.NativeMethods;

/**
 * Bridge used by {@link NormalizedMurmurHashEntropyProviderTest}. Provides access to the native
 * implementation of NormalizedMurmurHashEntropyProvider which is used as the source of truth for
 * the Java implementation.
 */
public class NormalizedMurmurHashEntropyProviderTestUtilsBridge {
    public static int murmurHash16(int seed, int data) {
        return NormalizedMurmurHashEntropyProviderTestUtilsBridgeJni.get().murmurHash16(seed, data);
    }

    public static double getEntropyForTrial(
            int randomizationSeed, int entropyValue, int entropyRange) {
        return NormalizedMurmurHashEntropyProviderTestUtilsBridgeJni.get()
                .getEntropyForTrial(randomizationSeed, entropyValue, entropyRange);
    }

    @NativeMethods
    interface Natives {
        int murmurHash16(int seed, int data);

        double getEntropyForTrial(int randomizationSeed, int entropyValue, int entropyRange);
    }
}
