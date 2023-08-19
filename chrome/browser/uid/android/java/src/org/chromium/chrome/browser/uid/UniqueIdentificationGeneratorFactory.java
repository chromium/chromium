// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.uid;

import java.util.HashMap;
import java.util.Map;

/**
 * Factory for setting and retrieving instances of {@link UniqueIdentificationGenerator}s.
 * <p/>
 * A generator must always be set for a generator type before it is asked for. A generator type
 * is any string you want to use for your generator. It is typically defined as a public static
 * field in the generator itself.
 */
public final class UniqueIdentificationGeneratorFactory {
    private static final Object LOCK = new Object();
    private static final Map<String, UniqueIdentificationGenerator> GENERATOR_MAP =
            new HashMap<String, UniqueIdentificationGenerator>();

    private UniqueIdentificationGeneratorFactory() {}

    /**
     * Returns a UniqueIdentificationGenerator if it exists, else throws IllegalArgumentException.
     *
     * @param generatorType the generator type you want
     * @return a unique ID generator
     */
    public static UniqueIdentificationGenerator getInstance(String generatorType) {
        synchronized (LOCK) {
            if (!GENERATOR_MAP.containsKey(generatorType)) {
                throw new IllegalArgumentException("Unknown generator type " + generatorType);
            }
            return GENERATOR_MAP.get(generatorType);
        }
    }

    /**
     * During startup of the application, and before any calls to
     * {@link #getInstance(String)} you must add all supported generators
     * to this factory.
     *
     * @param generatorType the type of generator this is. Must be a unique string.
     * @param gen           the generator.
     * @param force         if set to true, will override any existing generator for this type. Else
     *                      discards calls where a generator exists.
     */
    public static void registerGenerator(
            String generatorType, UniqueIdentificationGenerator gen, boolean force) {
        synchronized (LOCK) {
            if (GENERATOR_MAP.containsKey(generatorType) && !force) {
                return;
            }
            GENERATOR_MAP.put(generatorType, gen);
        }
    }

    public static void clearGeneratorMapForTest() {
        synchronized (LOCK) {
            GENERATOR_MAP.clear();
        }
    }
}
