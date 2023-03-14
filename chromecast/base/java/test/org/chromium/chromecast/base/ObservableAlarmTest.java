// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.mockito.ArgumentCaptor;

import org.chromium.chromecast.base.Observable.Scheduler;

/**
 * Tests for Observable#alarm().
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ObservableAlarmTest {
    @Test
    public void testAlarmDoesNotStartWithoutBeingSubscribedTo() {
        Scheduler scheduler = mock(Scheduler.class);
        Observable<?> a = Observable.alarm(scheduler, 100L);
        verify(scheduler, never()).postDelayed(any(), anyLong());
    }

    @Test
    public void testAlarmStartsForEachObserver() {
        Scheduler scheduler = mock(Scheduler.class);
        Observable<?> a = Observable.alarm(scheduler, 100L);
        ReactiveRecorder recorder1 = ReactiveRecorder.record(a);
        ReactiveRecorder recorder2 = ReactiveRecorder.record(a);
        recorder1.verify().end();
        recorder2.verify().end();
        verify(scheduler, times(2)).postDelayed(any(), eq(100L));
    }

    @Test
    public void testPostedRunnableResolvesCallback() {
        Scheduler scheduler = mock(Scheduler.class);
        ArgumentCaptor<Runnable> taskCaptor = ArgumentCaptor.forClass(Runnable.class);
        Observable<?> a = Observable.alarm(scheduler, 100L);
        ReactiveRecorder recorder = ReactiveRecorder.record(a);
        verify(scheduler).postDelayed(taskCaptor.capture(), eq(100L));
        Runnable delayedTask = taskCaptor.getValue();
        delayedTask.run();
        recorder.verify().opened(Unit.unit()).end();
    }

    @Test
    public void testDoesNotNotifyObserverAfterUnsubscribe() {
        Scheduler scheduler = mock(Scheduler.class);
        ArgumentCaptor<Runnable> taskCaptor = ArgumentCaptor.forClass(Runnable.class);
        Observable<?> a = Observable.alarm(scheduler, 100L);
        ReactiveRecorder recorder = ReactiveRecorder.record(a);
        verify(scheduler).postDelayed(taskCaptor.capture(), eq(100L));
        Runnable delayedTask = taskCaptor.getValue();
        recorder.unsubscribe();
        delayedTask.run();
        recorder.verify().end();
    }

    @Test
    public void testNotifiesObserverAsynchronouslyWhenDelayIsZero() {
        Scheduler scheduler = mock(Scheduler.class);
        ArgumentCaptor<Runnable> taskCaptor = ArgumentCaptor.forClass(Runnable.class);
        Observable<?> a = Observable.alarm(scheduler, 0L);
        ReactiveRecorder recorder = ReactiveRecorder.record(a);
        verify(scheduler).postDelayed(taskCaptor.capture(), eq(0L));
        recorder.verify().end();
        Runnable delayedTask = taskCaptor.getValue();
        delayedTask.run();
        recorder.verify().opened(Unit.unit()).end();
    }
}
