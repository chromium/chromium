// Copyright 2018 The Chromium Authors
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
public class ObservableAndTest {
    @Test
    public void testBothState_activateFirstDoesNotTrigger() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a.and(b));
        a.set("A");
        recorder.verify().end();
    }

    @Test
    public void testBothState_activateSecondDoesNotTrigger() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a.and(b));
        b.set("B");
        recorder.verify().end();
    }

    @Test
    public void testBothState_activateBothTriggers() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a.and(b));
        a.set("A");
        b.set("B");
        recorder.verify().opened(Both.both("A", "B")).end();
    }

    @Test
    public void testBothState_deactivateFirstAfterTrigger() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a.and(b));
        a.set("A");
        b.set("B");
        a.reset();
        recorder.verify().opened(Both.both("A", "B")).closed(Both.both("A", "B")).end();
    }

    @Test
    public void testBothState_deactivateSecondAfterTrigger() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a.and(b));
        a.set("A");
        b.set("B");
        b.reset();
        recorder.verify().opened(Both.both("A", "B")).closed(Both.both("A", "B")).end();
    }

    @Test
    public void testBothState_resetFirstBeforeSettingSecond_doesNotTrigger() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a.and(b));
        a.set("A");
        a.reset();
        b.set("B");
        recorder.verify().end();
    }

    @Test
    public void testBothState_resetSecondBeforeSettingFirst_doesNotTrigger() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a.and(b));
        b.set("B");
        b.reset();
        a.set("A");
        recorder.verify().end();
    }

    @Test
    public void testBothState_setOneControllerAfterTrigger_implicitlyResetsAndSets() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a.and(b));
        a.set("A1");
        b.set("B1");
        a.set("A2");
        b.set("B2");
        recorder.verify()
                .opened(Both.both("A1", "B1"))
                .closed(Both.both("A1", "B1"))
                .opened(Both.both("A2", "B1"))
                .closed(Both.both("A2", "B1"))
                .opened(Both.both("A2", "B2"))
                .end();
    }

    @Test
    public void testComposeBoth() {
        Controller<String> a = new Controller<>();
        Controller<String> b = new Controller<>();
        Controller<String> c = new Controller<>();
        Controller<String> d = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a.and(b).and(c).and(d));
        a.set("a");
        b.set("b");
        c.set("c");
        d.set("d");
        a.reset();
        recorder.verify()
                .opened(Both.both(Both.both(Both.both("a", "b"), "c"), "d"))
                .closed(Both.both(Both.both(Both.both("a", "b"), "c"), "d"))
                .end();
    }

    @Test
    public void testAndCartesianProduct() {
        Observable<Integer> numbers =
                observer -> observer.open(1).and(observer.open(2)).and(observer.open(3));
        Observable<String> letters =
                observer -> observer.open("a").and(observer.open("b")).and(observer.open("c"));
        ReactiveRecorder r = ReactiveRecorder.record(numbers.and(letters));
        r.verify()
                .opened(Both.both(1, "a"))
                .opened(Both.both(1, "b"))
                .opened(Both.both(1, "c"))
                .opened(Both.both(2, "a"))
                .opened(Both.both(2, "b"))
                .opened(Both.both(2, "c"))
                .opened(Both.both(3, "a"))
                .opened(Both.both(3, "b"))
                .opened(Both.both(3, "c"))
                .end();
        r.unsubscribe();
        r.verify()
                .closed(Both.both(3, "c"))
                .closed(Both.both(3, "b"))
                .closed(Both.both(3, "a"))
                .closed(Both.both(2, "c"))
                .closed(Both.both(2, "b"))
                .closed(Both.both(2, "a"))
                .closed(Both.both(1, "c"))
                .closed(Both.both(1, "b"))
                .closed(Both.both(1, "a"))
                .end();
    }
}
