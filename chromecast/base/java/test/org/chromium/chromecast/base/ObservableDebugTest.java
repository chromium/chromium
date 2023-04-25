// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.contains;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Batch;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for Observable#debug().
 */
@RunWith(BlockJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ObservableDebugTest {
    @Test
    public void testDebugScope() {
        Controller<Unit> a = new Controller<>();
        List<String> events = new ArrayList<>();
        Scope sub = a.debug(events::add).subscribe(x -> () -> {});
        assertThat(events, contains("subscribe"));
        events.clear();
        sub.close();
        assertThat(events, contains("unsubscribe"));
    }

    @Test
    public void testDebugUnitActivations() {
        Controller<Unit> a = new Controller<>();
        List<String> events = new ArrayList<>();
        Scope sub = a.debug(events::add).subscribe(x -> () -> {});
        events.clear();
        a.set(Unit.unit());
        assertThat(events, contains("open ()"));
        events.clear();
        a.reset();
        assertThat(events, contains("close ()"));
    }

    @Test
    public void testDebugsUnsubscribeBeforeClose() {
        // We can distinguish between a "close" event happening as the result of an "unsubscribe"
        // event and a "close" event happening on its own by whether the "unsubscribe" event happens
        // first.
        Controller<Unit> a = new Controller<>();
        List<String> events = new ArrayList<>();
        Scope sub = a.debug(events::add).subscribe(x -> () -> {});
        a.set(Unit.unit());
        events.clear();
        sub.close();
        assertThat(events, contains("unsubscribe", "close ()"));
    }

    @Test
    public void testDebugString() {
        Controller<String> a = new Controller<>();
        List<String> events = new ArrayList<>();
        Scope sub = a.debug(events::add).subscribe(x -> () -> {});
        events.clear();
        a.set("a");
        assertThat(events, contains("open a"));
        events.clear();
        a.set("b");
        assertThat(events, contains("close a", "open b"));
        events.clear();
        a.set("c");
        assertThat(events, contains("close b", "open c"));
    }

    @Test
    public void testDebugMultipleIntegers() {
        Observable<Integer> a =
                observer -> observer.open(1).and(observer.open(2)).and(observer.open(3));
        List<String> events = new ArrayList<>();
        Scope sub = a.debug(events::add).subscribe(x -> () -> {});
        assertThat(events, contains("subscribe", "open 1", "open 2", "open 3"));
        events.clear();
        sub.close();
        assertThat(events, contains("unsubscribe", "close 3", "close 2", "close 1"));
    }

    @Test
    public void testDebugDoesNotAffectData() {
        Controller<Integer> a = new Controller<>();
        ReactiveRecorder recorder = ReactiveRecorder.record(a.debug(x -> {}));
        recorder.verify().end();
        a.set(1);
        recorder.verify().opened(1).end();
        a.set(2);
        recorder.verify().closed(1).opened(2).end();
        a.reset();
        recorder.verify().closed(2).end();
        a.set(3);
        recorder.verify().opened(3).end();
        recorder.unsubscribe();
        recorder.verify().closed(3).end();
    }
}
