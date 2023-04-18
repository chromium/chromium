// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import androidx.annotation.Nullable;

/**
 * Implemented in Chromium.
 *
 * A simple key-value cache that is persisting all data on disk. Automatically evicts old data.
 */
public interface PersistentKeyValueCache {
    /** Consumes the result of PersistentKeyValueCache.lookup(). */
    public interface ValueConsumer {
        /**
         * Called when a lookup is complete.
         *
         * @param value The value found. null if no value was present.
         */
        void run(@Nullable byte[] value);
    }

    /**
     * Retrieves and returns the value associated with the given key if it exists.
     * This does not affect the key/value's age for automatic eviction.
     *
     * @param key The key to look up.
     * @param consumer The consumer called when the lookup is complete.
     */
    default void lookup(byte[] key, ValueConsumer consumer) {}

    /**
     * Inserts the given entry into the cache, overwriting any entry that might already exist
     * against the given key.
     *
     * @param key The key to insert.
     * @param value The value to insert.
     * @param onComplete Called after the key/value was inserted.
     */
    default void put(byte[] key, byte[] value, @Nullable Runnable onComplete) {}

    /**
     * Evicts an entry from the cache by the specified key.
     *
     * @param key The key whose entry must be evicted.
     * @param onComplete Called after the operation completes.
     */
    default void evict(byte[] key, @Nullable Runnable onComplete) {}
}
