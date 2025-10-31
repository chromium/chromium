// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import java.util.HashMap;

/**
 * A Map-like reactive container that can dynamically notify observers of updates to keys and
 * values.
 *
 * <p>Instead of reading the "current" state of the container, observers subscribe to the state
 * using the Observables returned from keys(), values(), or entries().
 *
 * <p>Like in java.util.Map, all keys in the dictionary are unique (based on `equals()`) and must
 * have a compliant `hashCode()` implementation.
 *
 * @param <K> The type of the keys in the dictionary.
 * @param <V> The type of the values in the dictionary.
 */
public class Dict<K, V> {
    private static class Entry<V> implements Scope {
        public final V value;
        public final Scope scope;

        Entry(V value, Scope scope) {
            this.value = value;
            this.scope = scope;
        }

        @Override
        public void close() {
            this.scope.close();
        }
    }

    private final Sequencer mSequencer = new Sequencer();
    private final HashMap<K, Entry<V>> mKeys = new HashMap<>();
    private final Pool<Both<K, V>> mEntries = new Pool<>();

    /**
     * Observe the entries (both keys and values) in the dictionary.
     *
     * <p>Observers are notified immediately of each of the entries in the dictionary, and will also
     * be notified when any entry is added or removed for as long as they are subscribed.
     */
    public Observable<Both<K, V>> entries() {
        return mEntries;
    }

    /**
     * Observe the keys in the dictionary.
     *
     * <p>Observers are notified immediately of each of the keys in the dictionary, and will also be
     * notified when any key is added or removed for as long as they are subscribed.
     *
     * <p>All the keys emitted by the Observable at a given time are guaranteed to be unique.
     */
    public Observable<K> keys() {
        return entries().map(Both::getFirst);
    }

    /**
     * Observe the values in the dictionary.
     *
     * <p>Observers are notified immediately of each of the values in the dictionary, and will also
     * be notified when any value is added or removed for as long as they are subscribed.
     *
     * <p>The values emitted by the Observable may not be unique; if multiple keys map to the same
     * value, that value will appear multiple times in the Observable returned by `values()`.
     */
    public Observable<V> values() {
        return entries().map(Both::getSecond);
    }

    /**
     * Associate a key with a value in the dictionary.
     *
     * <p>This will notify any observers that are subscribed of the new key/value.
     *
     * <p>If the key is already present in the dictionary, it and its associated value will first be
     * removed and observers notified of the removal. However, if the already-present key maps to
     * the same value (as decided by `equals()`) that is provided to `put()`, this method is a no-op
     * and observers will not be notified of any changes.
     */
    public void put(K key, V value) {
        mSequencer.sequence(
                () -> {
                    var entry = mKeys.get(key);
                    if (entry != null) {
                        // If the dictionary is unchanged because the key and value are the same as
                        // a key-value mapping that was already in the dictionary, do nothing, since
                        // the data are unchanged and observers don't need to be notified.
                        if (entry.value.equals(value)) {
                            return;
                        }
                        // If the dictionary value associated with the key needs to change, first
                        // close the scope associated with the previous entry, so observers
                        // retaining the old entry are notified of the update.
                        mKeys.remove(key).close();
                    }
                    mKeys.put(key, new Entry(value, mEntries.add(Both.of(key, value))));
                });
    }

    /**
     * Remove a key and its associated value from the dictionary.
     *
     * <p>This will notify any observers that are subscribed that the key/value is removed.
     *
     * <p>If the key is not present in the dictionary, this is a no-op.
     */
    public void remove(K key) {
        mSequencer.sequence(
                () -> {
                    if (mKeys.containsKey(key)) {
                        mKeys.remove(key).close();
                    }
                });
    }
}
