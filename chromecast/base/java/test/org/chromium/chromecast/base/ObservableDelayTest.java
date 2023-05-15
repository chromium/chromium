// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Batch;

import java.util.Map;
import java.util.TreeMap;
import java.util.function.BiFunction;

/**
 * Tests for Observable#delay().
 */
@RunWith(BlockJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ObservableDelayTest {
    // Helper function that inserts the key-value pair into a Map if the key isn't already in the
    // Map, or updates the value for the key by applying the combinator to the current value and the
    // new value if the key is already in the Map. This lets you turn a Map into a sort of MultiMap,
    // as long as you supply a monoidal combinator for the Map's value type.
    private static <K, V> void updateValue(Map<K, V> map, K key, V value,
            BiFunction<? super V, ? super V, ? extends V> combinator) {
        V current = map.get(key);
        map.put(key, current == null ? value : combinator.apply(current, value));
    }

    private static class FakeScheduler implements Observable.Scheduler {
        private long mCurrentTime;
        private final TreeMap<Long, Runnable> mTasks = new TreeMap<>();

        @Override
        public void postDelayed(Runnable runnable, long delay) {
            // Allow multiple tasks to be posted with the same delay by combining any
            // already-inserted Runnables with the newly-posted Runnable.
            updateValue(mTasks, mCurrentTime + delay, runnable, (a, b) -> () -> {
                a.run();
                b.run();
            });
        }

        public void fastForwardBy(long ms) {
            // Query for tasks that are scheduled for after the current time (exclusive), and before
            // what the current time will be after the given delay (inclusive). By making the start
            // time exclusive, we avoid running already-run tasks, while by making the end time
            // inclusive, we ensure fastForwardBy(n) runs tasks that were posted with a delay of n.
            mTasks.subMap(mCurrentTime, false, mCurrentTime + ms, true)
                    .values()
                    .forEach(Runnable::run);
            mCurrentTime += ms;
        }
    }

    @Test
    public void testDelayController() {
        FakeScheduler scheduler = new FakeScheduler();
        Controller<String> a = new Controller<>();
        a.set("a");
        ReactiveRecorder recorder = ReactiveRecorder.record(a.delay(scheduler, 100));
        recorder.verify().end();
        scheduler.fastForwardBy(100);
        recorder.verify().opened("a").end();
    }

    @Test
    public void testDoesNotNotifyBeforeDelayElapses() {
        FakeScheduler scheduler = new FakeScheduler();
        Controller<String> a = new Controller<>();
        a.set("a");
        ReactiveRecorder recorder = ReactiveRecorder.record(a.delay(scheduler, 100));
        recorder.verify().end();
        scheduler.fastForwardBy(99);
        recorder.verify().end();
    }

    @Test
    public void testDoesNotNotifyIfUnsubscribe() {
        FakeScheduler scheduler = new FakeScheduler();
        Controller<String> a = new Controller<>();
        a.set("a");
        ReactiveRecorder recorder = ReactiveRecorder.record(a.delay(scheduler, 100));
        recorder.unsubscribe();
        scheduler.fastForwardBy(100);
        recorder.verify().end();
    }

    @Test
    public void testAccumulateDelay() {
        FakeScheduler scheduler = new FakeScheduler();
        Controller<String> a = new Controller<>();
        a.set("a");
        ReactiveRecorder recorder = ReactiveRecorder.record(a.delay(scheduler, 100));
        scheduler.fastForwardBy(50);
        recorder.verify().end();
        scheduler.fastForwardBy(50);
        recorder.verify().opened("a").end();
    }

    @Test
    public void testDelayInt() {
        FakeScheduler scheduler = new FakeScheduler();
        ReactiveRecorder recorder =
                ReactiveRecorder.record(Observable.just(10).delay(scheduler, 100));
        scheduler.fastForwardBy(50);
        recorder.verify().end();
        scheduler.fastForwardBy(50);
        recorder.verify().opened(10).end();
    }

    @Test
    public void testDelayEmpty() {
        FakeScheduler scheduler = new FakeScheduler();
        ReactiveRecorder recorder =
                ReactiveRecorder.record(Observable.empty().delay(scheduler, 100));
        scheduler.fastForwardBy(100);
        recorder.verify().end();
    }

    @Test
    public void testDelayHigherCardinalityObservable() {
        FakeScheduler scheduler = new FakeScheduler();
        Observable<Integer> src =
                observer -> observer.open(10).and(observer.open(20)).and(observer.open(30));
        ReactiveRecorder recorder = ReactiveRecorder.record(src.delay(scheduler, 100));
        recorder.verify().end();
        scheduler.fastForwardBy(100);
        recorder.verify().opened(10).opened(20).opened(30).end();
        recorder.unsubscribe();
        recorder.verify().closed(30).closed(20).closed(10).end();
    }

    @Test
    public void testNewActivationResetsDelay() {
        FakeScheduler scheduler = new FakeScheduler();
        Controller<String> a = new Controller<>();
        a.set("a");
        ReactiveRecorder recorder = ReactiveRecorder.record(a.delay(scheduler, 100));
        scheduler.fastForwardBy(50);
        a.set("b");
        scheduler.fastForwardBy(50);
        recorder.verify().end();
        scheduler.fastForwardBy(50);
        recorder.verify().opened("b").end();
    }
}
