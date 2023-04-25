// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Batch;
import org.chromium.chromecast.base.Inheritance.Base;
import org.chromium.chromecast.base.Inheritance.Derived;

import java.util.ArrayList;
import java.util.List;

/**
 * Miscellaneous tests for Observable.
 *
 * This includes advanced behaviors like subscription-currying and correct use of generics.
 */
@RunWith(BlockJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ObservableMiscellaneousTest {
    @Test
    public void testMakeNotifyOneAtATime() {
        ReactiveRecorder r = ReactiveRecorder.record(observer -> {
            observer.open(1).close();
            observer.open(2).close();
            observer.open(3).close();
            return Scope.NO_OP;
        });
        r.verify().opened(1).closed(1).opened(2).closed(2).opened(3).closed(3).end();
    }

    @Test
    public void testMakeNotifyAllAtOnce() {
        ReactiveRecorder r = ReactiveRecorder.record(
                observer -> observer.open("a").and(observer.open("b")).and(observer.open("c")));
        r.verify().opened("a").opened("b").opened("c").end();
        r.unsubscribe();
        r.verify().closed("c").closed("b").closed("a").end();
    }

    @Test
    public void testEmpty() {
        ReactiveRecorder r = ReactiveRecorder.record(Observable.empty());
        r.verify().end();
        r.unsubscribe();
        r.verify().end();
    }

    @Test
    public void testAssignEmptyToTypedObservable() {
        Observable<String> a = Observable.empty();
        Observable<Integer> b = Observable.empty();
    }

    @Test
    public void testJustInt() {
        ReactiveRecorder r = ReactiveRecorder.record(Observable.just(100));
        r.verify().opened(100).end();
        r.unsubscribe();
        r.verify().closed(100).end();
    }

    @Test
    public void testJustString() {
        ReactiveRecorder r = ReactiveRecorder.record(Observable.just("hello"));
        r.verify().opened("hello").end();
        r.unsubscribe();
        r.verify().closed("hello").end();
    }

    @Test
    public void testBeingTooCleverWithObserversAndInheritance() {
        Controller<Base> baseController = new Controller<>();
        Controller<Derived> derivedController = new Controller<>();
        List<String> result = new ArrayList<>();
        // Test that the same Observer object can observe Observables of different types, as
        // long as the Observer type is a superclass of both Observable types.
        Observer<Base> observer = (Base value) -> {
            result.add("enter: " + value.toString());
            return () -> result.add("exit: " + value.toString());
        };
        baseController.subscribe(observer);
        // Compile error if generics are wrong.
        derivedController.subscribe(observer);
        baseController.set(new Base());
        // The scope from the previous set() call will not be overridden because this is activating
        // a different Controller.
        derivedController.set(new Derived());
        // The Controller<Base> can be activated with an object that extends Base.
        baseController.set(new Derived());
        assertThat(
                result, contains("enter: Base", "enter: Derived", "exit: Base", "enter: Derived"));
    }

    @Test
    public void testsubscribeCurrying() {
        Controller<String> aState = new Controller<>();
        Controller<String> bState = new Controller<>();
        Controller<String> result = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(result);
        // I guess this makes .and() obsolete?
        aState.subscribe(a -> bState.subscribe(b -> {
            result.set("" + a + ", " + b);
            return () -> result.reset();
        }));
        aState.set("A");
        bState.set("B");
        recorder.verify().opened("A, B").end();
        aState.reset();
        recorder.verify().closed("A, B").end();
        aState.set("AA");
        recorder.verify().opened("AA, B").end();
        bState.reset();
        recorder.verify().closed("AA, B").end();
    }

    @Test
    public void testPowerUnlimitedPower() {
        Controller<Unit> aState = new Controller<>();
        Controller<Unit> bState = new Controller<>();
        Controller<Unit> cState = new Controller<>();
        Controller<Unit> dState = new Controller<>();
        List<String> result = new ArrayList<>();
        // Praise be to Haskell Curry.
        aState.subscribe(a -> bState.subscribe(b -> cState.subscribe(c -> dState.subscribe(d -> {
            result.add("it worked!");
            return () -> result.add("exit");
        }))));
        aState.set(Unit.unit());
        bState.set(Unit.unit());
        cState.set(Unit.unit());
        dState.set(Unit.unit());
        assertThat(result, contains("it worked!"));
        result.clear();
        aState.reset();
        assertThat(result, contains("exit"));
        result.clear();
        aState.set(Unit.unit());
        assertThat(result, contains("it worked!"));
        result.clear();
        bState.reset();
        assertThat(result, contains("exit"));
        result.clear();
        bState.set(Unit.unit());
        assertThat(result, contains("it worked!"));
        result.clear();
        cState.reset();
        assertThat(result, contains("exit"));
        result.clear();
        cState.set(Unit.unit());
        assertThat(result, contains("it worked!"));
        result.clear();
        dState.reset();
        assertThat(result, contains("exit"));
        result.clear();
        dState.set(Unit.unit());
        assertThat(result, contains("it worked!"));
    }

    // Any Scope implementation with a constructor of one argument can use a method reference to its
    // constructor as an Observer.
    private static class TransitionLogger implements Scope {
        public static final List<String> sResult = new ArrayList<>();
        private final String mData;

        public TransitionLogger(String data) {
            mData = data;
            sResult.add("enter: " + mData);
        }

        @Override
        public void close() {
            sResult.add("exit: " + mData);
        }
    }

    @Test
    public void testObserverWithAutoCloseableConstructor() {
        Controller<String> controller = new Controller<>();
        // You can use a constructor method reference in a subscribe() call.
        controller.subscribe(TransitionLogger::new);
        controller.set("a");
        controller.reset();
        assertThat(TransitionLogger.sResult, contains("enter: a", "exit: a"));
    }
}
