// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Batch;

/**
 * Tests for Observable#and().
 */
@RunWith(BlockJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ObservableFoldTest {
    // Use fold() operator to construct a string that compactly represents all state transitions of
    // |src|. For each activation in |src|, the result adds a "+" followed by the data, and for each
    // deactivation in |src|, the result adds a "-" followed by the data.
    //
    // For example, "+a+b-b-a" means that |src| added "a", then added "b", then removed "b", then
    // removed "a".
    private static Observable<String> transitionString(Observable<String> src) {
        return src.fold("", (a, s) -> a + "+" + s, (a, s) -> a + "-" + s);
    }

    // Use fold() operator to construct a running sum of |src|. The data in the result will always
    // reflect the sum of all current activations in |src|.
    private static Observable<Integer> sum(Observable<Integer> src) {
        return src.fold(0, (a, n) -> a + n, (a, n) -> a - n);
    }

    @Test
    public void emptyInteger() {
        ReactiveRecorder r = ReactiveRecorder.record(sum(Observable.empty()));
        r.verify().opened(0).end();
        r.unsubscribe();
        r.verify().closed(0).end();
    }

    @Test
    public void emptyString() {
        ReactiveRecorder r = ReactiveRecorder.record(transitionString(Observable.empty()));
        r.verify().opened("").end();
        r.unsubscribe();
        r.verify().closed("").end();
    }

    @Test
    public void oneInteger() {
        ReactiveRecorder r = ReactiveRecorder.record(sum(Observable.just(10)));
        r.verify().opened(10).end();
        r.unsubscribe();
        r.verify().closed(10).end();
    }

    @Test
    public void oneString() {
        ReactiveRecorder r = ReactiveRecorder.record(transitionString(Observable.just("a")));
        r.verify().opened("+a").end();
        r.unsubscribe();
        r.verify().closed("+a").end();
    }

    @Test
    public void multipleIntegers() {
        ReactiveRecorder r = ReactiveRecorder.record(sum(
                observer -> observer.open(1).and(observer.open(2)).and(observer.open(3))));
        r.verify().opened(6).end();
        r.unsubscribe();
        r.verify().closed(6).end();
    }

    @Test
    public void multipleStrings() {
        ReactiveRecorder r = ReactiveRecorder.record(transitionString(
                observer -> observer.open("a").and(observer.open("b")).and(observer.open("c"))));
        r.verify().opened("+a+b+c").end();
        r.unsubscribe();
        r.verify().closed("+a+b+c").end();
    }

    @Test
    public void mutableInteger() {
        Cell<Integer> src = new Cell<>(0);
        ReactiveRecorder r = ReactiveRecorder.record(sum(src));
        r.verify().opened(0).end();
        src.set(10);
        r.verify().closed(0).opened(10).end();
        src.set(20);
        r.verify().closed(10).opened(0).closed(0).opened(20).end();
    }

    @Test
    public void mutableString() {
        Cell<String> src = new Cell<>("a");
        ReactiveRecorder r = ReactiveRecorder.record(transitionString(src));
        r.verify().opened("+a").end();
        src.set("b");
        r.verify().closed("+a").opened("+a-a").closed("+a-a").opened("+a-a+b").end();
        src.set("c");
        r.verify()
                .closed("+a-a+b")
                .opened("+a-a+b-b")
                .closed("+a-a+b-b")
                .opened("+a-a+b-b+c")
                .end();
    }
}
