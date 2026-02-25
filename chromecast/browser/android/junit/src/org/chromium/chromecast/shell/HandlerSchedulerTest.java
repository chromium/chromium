// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import static org.junit.Assert.assertEquals;

import android.os.Handler;
import android.os.Looper;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chromecast.base.Box;
import org.chromium.chromecast.base.Observable.Scheduler;

import java.util.concurrent.TimeUnit;

/** Tests for HandlerScheduler. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HandlerSchedulerTest {
    @Test
    public void testInjectedHandler() {
        Looper looper = Looper.getMainLooper();
        Handler handler = new Handler(looper);
        Scheduler scheduler = HandlerScheduler.fromHandler(handler);
        Box<Integer> box = new Box<>(0);
        scheduler.postDelayed(() -> ++box.value, 100);
        assertEquals(0, (int) box.value);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(1, (int) box.value);
        scheduler.postDelayed(() -> ++box.value, 100);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(2, (int) box.value);
    }

    @Test
    public void testOnCurrentThread() {
        Scheduler scheduler = HandlerScheduler.onCurrentThread();
        Box<Integer> box = new Box<>(0);
        scheduler.postDelayed(() -> ++box.value, 100);
        assertEquals(0, (int) box.value);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(1, (int) box.value);
        scheduler.postDelayed(() -> ++box.value, 100);
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertEquals(2, (int) box.value);
    }

    @Test
    public void testDelayInterval() {
        Scheduler scheduler = HandlerScheduler.onCurrentThread();
        Box<Integer> box = new Box<>(0);
        scheduler.postDelayed(() -> ++box.value, 100);
        ShadowLooper.idleMainLooper(50, TimeUnit.MILLISECONDS);
        assertEquals(0, (int) box.value);
        ShadowLooper.idleMainLooper(50, TimeUnit.MILLISECONDS);
        assertEquals(1, (int) box.value);
        scheduler.postDelayed(() -> ++box.value, 100);
        ShadowLooper.idleMainLooper(50, TimeUnit.MILLISECONDS);
        assertEquals(1, (int) box.value);
        ShadowLooper.idleMainLooper(50, TimeUnit.MILLISECONDS);
        assertEquals(2, (int) box.value);
    }
}
