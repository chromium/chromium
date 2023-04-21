// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.junit.Assert.assertEquals;

import android.os.Handler;
import android.os.Looper;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.chromecast.base.Box;
import org.chromium.chromecast.base.Observable.Scheduler;

/**
 * Tests for HandlerScheduler.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class HandlerSchedulerTest {
    @Test
    public void testInjectedHandler() {
        Looper looper = Looper.getMainLooper();
        Handler handler = new Handler(looper);
        Scheduler scheduler = HandlerScheduler.fromHandler(handler);
        Box<Integer> box = new Box<>(0);
        scheduler.postDelayed(() -> ++box.value, 100);
        assertEquals(0, (int) box.value);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(1, (int) box.value);
        scheduler.postDelayed(() -> ++box.value, 100);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(2, (int) box.value);
    }

    @Test
    public void testOnCurrentThread() {
        Scheduler scheduler = HandlerScheduler.onCurrentThread();
        Box<Integer> box = new Box<>(0);
        scheduler.postDelayed(() -> ++box.value, 100);
        assertEquals(0, (int) box.value);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(1, (int) box.value);
        scheduler.postDelayed(() -> ++box.value, 100);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertEquals(2, (int) box.value);
    }

    @Test
    public void testDelayInterval() {
        ShadowLooper shadowLooper = Shadows.shadowOf(Looper.getMainLooper());
        org.robolectric.util.Scheduler robolectricScheduler = shadowLooper.getScheduler();
        Scheduler scheduler = HandlerScheduler.onCurrentThread();
        Box<Integer> box = new Box<>(0);
        scheduler.postDelayed(() -> ++box.value, 100);
        robolectricScheduler.advanceBy(50);
        assertEquals(0, (int) box.value);
        robolectricScheduler.advanceBy(50);
        assertEquals(1, (int) box.value);
        scheduler.postDelayed(() -> ++box.value, 100);
        robolectricScheduler.advanceBy(50);
        assertEquals(1, (int) box.value);
        robolectricScheduler.advanceBy(50);
        assertEquals(2, (int) box.value);
    }
}
