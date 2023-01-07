// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/**
 * Tests for Observable#flatMap().
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ObservableFlatMapTest {
    @Test
    public void testFlatMapWithIdentity() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        Controller<Controller<String>> src = new Controller<>();
        ReactiveRecorder r = ReactiveRecorder.record(src.flatMap(x -> x));
        r.verify().end();
        src.set(a);
        r.verify().end();
        a.set("a");
        r.verify().opened("a").end();
        a.set("A");
        r.verify().closed("a").opened("A").end();
        b.set("b");
        r.verify().end();
        src.set(b);
        r.verify().closed("A").opened("b").end();
        src.set(a);
        r.verify().closed("b").opened("A").end();
    }

    private static class Person {
        public final Observable<String> name;
        public final Observable<Integer> age;

        Person(Observable<String> name, Observable<Integer> age) {
            this.name = name;
            this.age = age;
        }
    }

    @Test
    public void testFlatMapWithAccessor() {
        Controller<Person> src = new Controller<>();
        ReactiveRecorder r =
                ReactiveRecorder.record(src.flatMap(person -> person.name.and(person.age)));
        r.verify().end();
        src.set(new Person(Observable.just("Alice"), Observable.just(30)));
        r.verify().opened(Both.both("Alice", 30)).end();
        src.set(new Person(Observable.just("Bob"), Observable.just(29)));
        r.verify().closed(Both.both("Alice", 30)).opened(Both.both("Bob", 29)).end();
    }
}
