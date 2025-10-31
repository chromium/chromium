// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.chromium.chromecast.base.ReactiveRecorder.record;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Batch;

@RunWith(BlockJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class DictTest {
    private static <A, B> Both<A, B> both(A a, B b) {
        return Both.of(a, b);
    }

    @Test
    public void emptyDict() {
        Dict<String, String> dict = new Dict<>();
        record(dict.entries()).verify().end();
        record(dict.keys()).verify().end();
        record(dict.values()).verify().end();
    }

    @Test
    public void oneIntToOneInt() {
        Dict<Integer, Integer> dict = new Dict<>();
        dict.put(1, 2);
        record(dict.entries()).verify().opened(both(1, 2)).end();
        record(dict.keys()).verify().opened(1).end();
        record(dict.values()).verify().opened(2).end();
    }

    @Test
    public void oneIntToOneString() {
        Dict<Integer, String> dict = new Dict<>();
        dict.put(1, "one");
        record(dict.entries()).verify().opened(both(1, "one")).end();
        record(dict.keys()).verify().opened(1).end();
        record(dict.values()).verify().opened("one").end();
    }

    @Test
    public void oneStringToOneInt() {
        var dict = new Dict<String, Integer>();
        dict.put("one", 1);
        record(dict.entries()).verify().opened(both("one", 1)).end();
        record(dict.keys()).verify().opened("one").end();
        record(dict.values()).verify().opened(1).end();
    }

    @Test
    public void observersAreNotifiedOfNewEntries() {
        var dict = new Dict<Integer, Integer>();
        var entries = record(dict.entries());
        var keys = record(dict.keys());
        var values = record(dict.values());

        entries.verify().end();
        keys.verify().end();
        values.verify().end();

        dict.put(1, 10);
        entries.verify().opened(both(1, 10)).end();
        keys.verify().opened(1).end();
        values.verify().opened(10).end();

        dict.put(2, 20);
        entries.verify().opened(both(2, 20)).end();
        keys.verify().opened(2).end();
        values.verify().opened(20).end();

        dict.put(3, 30);
        entries.verify().opened(both(3, 30)).end();
        keys.verify().opened(3).end();
        values.verify().opened(30).end();
    }

    @Test
    public void observersAreNotifiedOfRemovedEntries() {
        var dict = new Dict<String, String>();
        dict.put("a", "apple");
        dict.put("b", "banana");
        dict.put("c", "cranberry");
        dict.put("d", "durian");

        var entries = record(dict.entries());
        var keys = record(dict.keys());
        var values = record(dict.values());

        entries.verify()
                .opened(both("a", "apple"))
                .opened(both("b", "banana"))
                .opened(both("c", "cranberry"))
                .opened(both("d", "durian"))
                .end();
        keys.verify().opened("a").opened("b").opened("c").opened("d").end();
        values.verify().opened("apple").opened("banana").opened("cranberry").opened("durian").end();

        dict.remove("b");
        entries.verify().closed(both("b", "banana")).end();
        keys.verify().closed("b").end();
        values.verify().closed("banana").end();
    }

    @Test
    public void removeNonexistentKey() {
        var dict = new Dict<String, Integer>();
        dict.put("Alice", 5);
        dict.put("Bob", 3);

        var entries = record(dict.entries());
        var keys = record(dict.keys());
        var values = record(dict.values());

        entries.verify().opened(both("Alice", 5)).opened(both("Bob", 3)).end();
        keys.verify().opened("Alice").opened("Bob").end();
        values.verify().opened(5).opened(3).end();

        dict.remove("Eve");
        entries.verify().end();
        keys.verify().end();
        values.verify().end();
    }

    @Test
    public void overwritingAnEntryRemovesTheOldEntry() {
        var dict = new Dict<String, String>();

        var entries = record(dict.entries());
        var keys = record(dict.keys());
        var values = record(dict.values());

        dict.put("Alice", "Bob");
        entries.verify().opened(both("Alice", "Bob")).end();
        keys.verify().opened("Alice").end();
        values.verify().opened("Bob").end();

        dict.put("Alice", "Charlie");
        entries.verify().closed(both("Alice", "Bob")).opened(both("Alice", "Charlie")).end();
        keys.verify().closed("Alice").opened("Alice").end();
        values.verify().closed("Bob").opened("Charlie").end();
    }

    @Test
    public void closeOnUnsubscription() {
        var dict = new Dict<String, String>();

        dict.put("a", "A");
        dict.put("b", "B");
        dict.put("c", "C");

        var entries = record(dict.entries());
        var keys = record(dict.keys());
        var values = record(dict.values());

        entries.verify().opened(both("a", "A")).opened(both("b", "B")).opened(both("c", "C")).end();
        keys.verify().opened("a").opened("b").opened("c").end();
        values.verify().opened("A").opened("B").opened("C").end();

        // Data should be cleared in the reverse order that it was added.
        entries.unsubscribe();
        entries.verify().closed(both("c", "C")).closed(both("b", "B")).closed(both("a", "A")).end();
        keys.unsubscribe();
        keys.verify().closed("c").closed("b").closed("a").end();
        values.unsubscribe();
        values.verify().closed("C").closed("B").closed("A").end();
    }

    @Test
    public void putWithSameKeyAndValueIsIdempotent() {
        var dict = new Dict<String, Integer>();

        var entries = record(dict.entries());
        var keys = record(dict.keys());
        var values = record(dict.values());

        dict.put("asdf", 10);
        entries.verify().opened(both("asdf", 10)).end();
        keys.verify().opened("asdf").end();
        values.verify().opened(10).end();

        dict.put("asdf", 10);
        entries.verify().end();
        keys.verify().end();
        values.verify().end();
    }
}
