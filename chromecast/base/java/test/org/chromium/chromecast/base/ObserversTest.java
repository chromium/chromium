// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.emptyIterable;
import static org.junit.Assert.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.chromecast.base.Inheritance.Base;
import org.chromium.chromecast.base.Inheritance.Derived;

import java.util.ArrayList;
import java.util.List;
import java.util.function.BiFunction;
import java.util.function.Consumer;

/**
 * Tests for Observers, a utility class to construct readable objects that can be passed to
 * Observable#subscribe().
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ObserversTest {
    @Test
    public void testOnEnterWithConsumer() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.subscribe(Observers.onEnter((String s) -> result.add(s + ": got it!")));
        controller.set("thing");
        assertThat(result, contains("thing: got it!"));
    }

    @Test
    public void testOnEnterWithConsumerOfSuperclass() {
        Controller<Derived> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        Consumer<Base> consumer = (Base base) -> result.add(base.toString() + ": got it!");
        // Compile error if generics are wrong.
        Observer<Derived> observer = Observers.onEnter(consumer);
        controller.subscribe(observer);
        controller.set(new Derived());
        assertThat(result, contains("Derived: got it!"));
    }


    @Test
    public void testOnEnterMultipleActivations() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.subscribe(Observers.onEnter(s -> result.add(s.toString())));
        controller.set("a");
        controller.set("b");
        controller.set("c");
        assertThat(result, contains("a", "b", "c"));
    }

    @Test
    public void testOnExitNotFiredIfObservableIsNotDeactivated() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.subscribe(Observers.onExit((String s) -> result.add(s + ": got it!")));
        controller.set("stuff");
        assertThat(result, emptyIterable());
    }

    @Test
    public void testOnExitWithConsumer() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.subscribe(Observers.onExit((String s) -> result.add(s + ": got it!")));
        controller.set("thing");
        controller.reset();
        assertThat(result, contains("thing: got it!"));
    }

    @Test
    public void testOnExitWithConsumerOfSuperclass() {
        Controller<Derived> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        Consumer<Base> consumer = (Base base) -> result.add(base.toString() + ": got it!");
        // Compile error if generics are wrong.
        Observer<Derived> observer = Observers.onExit(consumer);
        controller.subscribe(observer);
        controller.set(new Derived());
        controller.reset();
        assertThat(result, contains("Derived: got it!"));
    }

    @Test
    public void testOnExitMultipleActivations() {
        Controller<String> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.subscribe(Observers.onExit(s -> result.add(s.toString())));
        controller.set("a");
        // Implicit reset causes exit handler to fire for "a".
        controller.set("b");
        // Implicit reset causes exit handler to fire for "b".
        controller.set("c");
        assertThat(result, contains("a", "b"));
    }

    @Test
    public void testHowUsingBothOnEnterAndOnExitLooks() {
        Controller<Derived> controller = new Controller<>();
        List<String> result = new ArrayList<>();
        controller.subscribe(Observers.onEnter((Base base) -> result.add("enter " + base)));
        controller.subscribe(Observers.onExit((Base base) -> result.add("exit " + base)));
        controller.set(new Derived());
        controller.reset();
        assertThat(result, contains("enter Derived", "exit Derived"));
    }

    @Test
    public void testsubscribeBothWithStrings() {
        Controller<String> controllerA = new Controller<>();
        Controller<String> controllerB = new Controller<>();
        List<String> result = new ArrayList<>();
        controllerA.and(controllerB).subscribe(Observers.both((String a, String b) -> {
            result.add("enter: " + a + ", " + b);
            return () -> result.add("exit: " + a + ", " + b);
        }));
        controllerA.set("A");
        controllerB.set("B");
        controllerA.set("AA");
        controllerB.set("BB");
        assertThat(result,
                contains("enter: A, B", "exit: A, B", "enter: AA, B", "exit: AA, B",
                        "enter: AA, BB"));
    }

    @Test
    public void testBuildObserverWithFunctionThatTakesSuperclass() {
        BiFunction<Base, Base, Scope> function = (Base a, Base b) -> {
            return () -> {};
        };
        // Compile error if generics are wrong.
        Observer<Both<Derived, Derived>> observer = Observers.both(function);
    }

    // Dummy class that extends Scope.
    private static class SubScope implements Scope {
        @Override
        public void close() {}
    }

    @Test
    public void testBuildObserverWithFunctionThatReturnsSubclassOfScope() {
        SubScope subScope = new SubScope();
        BiFunction<String, String, SubScope> function = (a, b) -> subScope;
        // Compile error if generics are wrong.
        Observer<Both<String, String>> observer = Observers.both(function);
    }
}
