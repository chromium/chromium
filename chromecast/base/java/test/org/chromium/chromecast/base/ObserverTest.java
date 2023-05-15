// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.emptyIterable;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Batch;
import org.chromium.chromecast.base.Inheritance.Base;
import org.chromium.chromecast.base.Inheritance.Derived;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;

/**
 * Tests for Observers, a utility class to construct readable objects that can be passed to
 * Observable#subscribe().
 */
@RunWith(BlockJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ObserverTest {
    @Test
    public void testonOpenWithConsumer() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.subscribe(Observer.onOpen((String s) -> result.add(s + ": got it!")));
        controller.set("thing");
        assertThat(result, contains("thing: got it!"));
    }

    @Test
    public void testonOpenWithConsumerOfSuperclass() {
        Controller<Derived> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        Consumer<Base> consumer = (Base base) -> result.add(base.toString() + ": got it!");
        // Compile error if generics are wrong.
        Observer<Derived> observer = Observer.onOpen(consumer);
        controller.subscribe(observer);
        controller.set(new Derived());
        assertThat(result, contains("Derived: got it!"));
    }

    @Test
    public void testonOpenMultipleActivations() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.subscribe(Observer.onOpen(s -> result.add(s.toString())));
        controller.set("a");
        controller.set("b");
        controller.set("c");
        assertThat(result, contains("a", "b", "c"));
    }

    @Test
    public void testonCloseNotFiredIfObservableIsNotDeactivated() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.subscribe(Observer.onClose((String s) -> result.add(s + ": got it!")));
        controller.set("stuff");
        assertThat(result, emptyIterable());
    }

    @Test
    public void testonCloseWithConsumer() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.subscribe(Observer.onClose((String s) -> result.add(s + ": got it!")));
        controller.set("thing");
        controller.reset();
        assertThat(result, contains("thing: got it!"));
    }

    @Test
    public void testonCloseWithConsumerOfSuperclass() {
        Controller<Derived> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        Consumer<Base> consumer = (Base base) -> result.add(base.toString() + ": got it!");
        // Compile error if generics are wrong.
        Observer<Derived> observer = Observer.onClose(consumer);
        controller.subscribe(observer);
        controller.set(new Derived());
        controller.reset();
        assertThat(result, contains("Derived: got it!"));
    }

    @Test
    public void testonCloseMultipleActivations() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.subscribe(Observer.onClose(s -> result.add(s.toString())));
        controller.set("a");
        // Implicit reset causes exit handler to fire for "a".
        controller.set("b");
        // Implicit reset causes exit handler to fire for "b".
        controller.set("c");
        assertThat(result, contains("a", "b"));
    }

    @Test
    public void testHowUsingBothonOpenAndonCloseLooks() {
        Controller<Derived> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.subscribe(Observer.onOpen((Base base) -> result.add("enter " + base)));
        controller.subscribe(Observer.onClose((Base base) -> result.add("exit " + base)));
        controller.set(new Derived());
        controller.reset();
        assertThat(result, contains("enter Derived", "exit Derived"));
    }

    @Test
    public void testBothAdaptObserver() {
        Cell<Integer> ints = new Cell<>(0);
        Cell<String> strings = new Cell<>("a");
        List<String> result = new ArrayList<>();
        Scope sub = ints.and(strings).subscribe(Observer.both((Integer i, String s) -> {
            result.add("opened " + i + " " + s);
            return () -> result.add("closed " + i + " " + s);
        }));
        assertThat(result, contains("opened 0 a"));
        result.clear();
        ints.set(1);
        assertThat(result, contains("closed 0 a", "opened 1 a"));
        result.clear();
        strings.set("b");
        assertThat(result, contains("closed 1 a", "opened 1 b"));
        result.clear();
        sub.close();
        assertThat(result, contains("closed 1 b"));
    }
}
