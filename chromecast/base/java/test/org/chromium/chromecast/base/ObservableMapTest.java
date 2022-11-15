// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.chromecast.base.Inheritance.Base;
import org.chromium.chromecast.base.Inheritance.Derived;

import java.util.function.Function;

/**
 * Tests for Observable#map().
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ObservableMapTest {
    @Test
    public void testMapController() {
        Controller<String> original = new Controller<>();
        Observable<String> lowerCase = original.map(String::toLowerCase);
        Observable<String> upperCase = lowerCase.map(String::toUpperCase);
        ReactiveRecorder recordOriginal = ReactiveRecorder.record(original);
        ReactiveRecorder recordLowerCase = ReactiveRecorder.record(lowerCase);
        ReactiveRecorder recordUpperCase = ReactiveRecorder.record(upperCase);
        original.set("sImPlY sTeAmEd KaLe");
        original.reset();
        recordOriginal.verify().opened("sImPlY sTeAmEd KaLe").closed("sImPlY sTeAmEd KaLe").end();
        recordLowerCase.verify().opened("simply steamed kale").closed("simply steamed kale").end();
        recordUpperCase.verify().opened("SIMPLY STEAMED KALE").closed("SIMPLY STEAMED KALE").end();
    }

    @Test
    public void testMapWithFunctionOfSuper() {
        Controller<Derived> a = new Controller<>();
        // Compile error if generics are wrong.
        Observable<String> r = a.map((Base base) -> base.toString());
        ReactiveRecorder recorder = ReactiveRecorder.record(r);
        a.set(new Derived());
        recorder.verify().opened("Derived").end();
    }

    @Test
    public void testMapReturnSubclassOfResultType() {
        Controller<Unit> a = new Controller<>();
        Derived d = new Derived();
        Function<Unit, Derived> f = x -> d;
        // Compile error if generics are wrong.
        Observable<Base> r = a.map(f);
        ReactiveRecorder recorder = ReactiveRecorder.record(r);
        a.set(Unit.unit());
        recorder.verify().opened(d).end();
    }

    @Test
    public void testMapDropsNullResult() {
        Controller<Unit> controller = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(controller.map(x -> null));
        controller.set(Unit.unit());
        // Recorder should not get any events because the map function returned null.
        recorder.verify().end();
    }
}
