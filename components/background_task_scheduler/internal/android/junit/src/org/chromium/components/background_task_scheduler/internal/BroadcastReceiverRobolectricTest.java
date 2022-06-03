// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import static org.junit.Assert.assertEquals;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.os.BatteryManager;
import android.os.Build;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowBatteryManager;
import org.robolectric.shadows.ShadowConnectivityManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.BackgroundTaskFactory;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;
import org.chromium.components.background_task_scheduler.TaskParameters;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Robolectric tests for BackgroundTaskBroadcastReceiver. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowBatteryManager.class, ShadowConnectivityManager.class})
public final class BroadcastReceiverRobolectricTest {
    private static final long WAIT_TIME_MS = 10;
    private CountDownLatch mScheduleLatch;
    private int mStopped;
    private int mRescheduled;

    private BatteryManager mBatteryManager;
    private ShadowBatteryManager mShadowBatteryManager;
    private ConnectivityManager mConnectivityManager;
    private ShadowConnectivityManager mShadowConnectivityManager;

    class TestBackgroundTask implements BackgroundTask {
        public TestBackgroundTask() {}

        @Override
        public boolean onStartTask(
                Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
            ThreadUtils.assertOnUiThread();
            mScheduleLatch.countDown();
            return false;
        }

        @Override
        public boolean onStopTask(Context context, TaskParameters taskParameters) {
            ThreadUtils.assertOnUiThread();
            mStopped++;
            return false;
        }

        @Override
        public void reschedule(Context context) {
            ThreadUtils.assertOnUiThread();
            mRescheduled++;
        }
    }

    class TestBackgroundTaskFactory implements BackgroundTaskFactory {
        @Override
        public BackgroundTask getBackgroundTaskFromTaskId(int taskId) {
            if (taskId == TaskIds.TEST) {
                return new TestBackgroundTask();
            }
            return null;
        }
    }

    @Before
    public void setUp() {
        mScheduleLatch = new CountDownLatch(1);
        mStopped = 0;
        mRescheduled = 0;

        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskFactory());

        mBatteryManager = (BatteryManager) ContextUtils.getApplicationContext().getSystemService(
                Context.BATTERY_SERVICE);
        mShadowBatteryManager = shadowOf(mBatteryManager);

        mConnectivityManager =
                (ConnectivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.CONNECTIVITY_SERVICE);
        mShadowConnectivityManager = shadowOf(mConnectivityManager);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testStartExact() throws InterruptedException {
        TaskInfo.TimingInfo exactInfo =
                TaskInfo.ExactInfo.create().setTriggerAtMs(System.currentTimeMillis()).build();
        TaskInfo exactTask = TaskInfo.createTask(TaskIds.TEST, exactInfo).build();
        BackgroundTaskSchedulerPrefs.addScheduledTask(exactTask);

        Intent intent = new Intent(
                ContextUtils.getApplicationContext(), BackgroundTaskBroadcastReceiver.class)
                                .putExtra(BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_ID_KEY,
                                        TaskIds.TEST);

        BackgroundTaskBroadcastReceiver receiver = new BackgroundTaskBroadcastReceiver();
        receiver.onReceive(ContextUtils.getApplicationContext(), intent);

        Assert.assertTrue(mScheduleLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
        assertEquals(0, mStopped);
        assertEquals(0, mRescheduled);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void noChargingRequired() throws InterruptedException {
        TaskInfo.TimingInfo exactInfo =
                TaskInfo.ExactInfo.create().setTriggerAtMs(System.currentTimeMillis()).build();
        TaskInfo exactTask =
                TaskInfo.createTask(TaskIds.TEST, exactInfo).setRequiresCharging(false).build();
        BackgroundTaskSchedulerPrefs.addScheduledTask(exactTask);

        Intent intent = new Intent(
                ContextUtils.getApplicationContext(), BackgroundTaskBroadcastReceiver.class)
                                .putExtra(BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_ID_KEY,
                                        TaskIds.TEST);

        BackgroundTaskBroadcastReceiver receiver = new BackgroundTaskBroadcastReceiver();
        receiver.onReceive(ContextUtils.getApplicationContext(), intent);

        Assert.assertTrue(mScheduleLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
        assertEquals(0, mStopped);
        assertEquals(0, mRescheduled);
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Feature({"BackgroundTaskScheduler"})
    public void withChargingRequired() throws InterruptedException {
        // Set device in charging mode
        mShadowBatteryManager.setIsCharging(true);
        Assert.assertTrue(mBatteryManager.isCharging());

        TaskInfo.TimingInfo exactInfo =
                TaskInfo.ExactInfo.create().setTriggerAtMs(System.currentTimeMillis()).build();
        TaskInfo exactTask =
                TaskInfo.createTask(TaskIds.TEST, exactInfo).setRequiresCharging(true).build();
        BackgroundTaskSchedulerPrefs.addScheduledTask(exactTask);

        Intent intent = new Intent(
                ContextUtils.getApplicationContext(), BackgroundTaskBroadcastReceiver.class)
                                .putExtra(BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_ID_KEY,
                                        TaskIds.TEST);

        BackgroundTaskBroadcastReceiver receiver = new BackgroundTaskBroadcastReceiver();
        receiver.onReceive(ContextUtils.getApplicationContext(), intent);

        Assert.assertTrue(mScheduleLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
        assertEquals(0, mStopped);
        assertEquals(0, mRescheduled);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void noNetworkRequirements() throws InterruptedException {
        TaskInfo.TimingInfo exactInfo =
                TaskInfo.ExactInfo.create().setTriggerAtMs(System.currentTimeMillis()).build();
        TaskInfo exactTask = TaskInfo.createTask(TaskIds.TEST, exactInfo)
                                     .setRequiredNetworkType(TaskInfo.NetworkType.NONE)
                                     .build();
        BackgroundTaskSchedulerPrefs.addScheduledTask(exactTask);

        Intent intent = new Intent(
                ContextUtils.getApplicationContext(), BackgroundTaskBroadcastReceiver.class)
                                .putExtra(BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_ID_KEY,
                                        TaskIds.TEST);

        BackgroundTaskBroadcastReceiver receiver = new BackgroundTaskBroadcastReceiver();
        receiver.onReceive(ContextUtils.getApplicationContext(), intent);

        Assert.assertTrue(mScheduleLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
        assertEquals(0, mStopped);
        assertEquals(0, mRescheduled);
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.L)
    @Feature({"BackgroundTaskScheduler"})
    public void withAnyNetworkRequired() throws InterruptedException {
        mShadowConnectivityManager.setDefaultNetworkActive(true);
        TaskInfo.TimingInfo exactInfo =
                TaskInfo.ExactInfo.create().setTriggerAtMs(System.currentTimeMillis()).build();
        TaskInfo exactTask = TaskInfo.createTask(TaskIds.TEST, exactInfo)
                                     .setRequiredNetworkType(TaskInfo.NetworkType.ANY)
                                     .build();
        BackgroundTaskSchedulerPrefs.addScheduledTask(exactTask);

        Intent intent = new Intent(
                ContextUtils.getApplicationContext(), BackgroundTaskBroadcastReceiver.class)
                                .putExtra(BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_ID_KEY,
                                        TaskIds.TEST);

        BackgroundTaskBroadcastReceiver receiver = new BackgroundTaskBroadcastReceiver();
        receiver.onReceive(ContextUtils.getApplicationContext(), intent);

        Assert.assertTrue(mScheduleLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
        assertEquals(0, mStopped);
        assertEquals(0, mRescheduled);
    }

    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Feature({"BackgroundTaskScheduler"})
    public void withAnyNetworkRequiredButNoConnectivity() throws InterruptedException {
        mShadowConnectivityManager.setDefaultNetworkActive(false);

        TaskInfo.TimingInfo exactInfo =
                TaskInfo.ExactInfo.create().setTriggerAtMs(System.currentTimeMillis()).build();
        TaskInfo exactTask = TaskInfo.createTask(TaskIds.TEST, exactInfo)
                                     .setRequiredNetworkType(TaskInfo.NetworkType.ANY)
                                     .build();
        BackgroundTaskSchedulerPrefs.addScheduledTask(exactTask);

        Intent intent = new Intent(
                ContextUtils.getApplicationContext(), BackgroundTaskBroadcastReceiver.class)
                                .putExtra(BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_ID_KEY,
                                        TaskIds.TEST);

        BackgroundTaskBroadcastReceiver receiver = new BackgroundTaskBroadcastReceiver();
        receiver.onReceive(ContextUtils.getApplicationContext(), intent);

        // TODO(crbug.com/1964613): Explore ways to avoid waiting for WAIT_TIME_MS here.
        Assert.assertFalse(mScheduleLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
        assertEquals(0, mStopped);
        assertEquals(0, mRescheduled);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void withUnmeteredNetworkRequired() throws InterruptedException {
        mShadowConnectivityManager.setActiveNetworkInfo(
                mConnectivityManager.getNetworkInfo(ConnectivityManager.TYPE_WIFI));
        TaskInfo.TimingInfo exactInfo =
                TaskInfo.ExactInfo.create().setTriggerAtMs(System.currentTimeMillis()).build();
        TaskInfo exactTask = TaskInfo.createTask(TaskIds.TEST, exactInfo)
                                     .setRequiredNetworkType(TaskInfo.NetworkType.UNMETERED)
                                     .build();
        BackgroundTaskSchedulerPrefs.addScheduledTask(exactTask);

        Intent intent = new Intent(
                ContextUtils.getApplicationContext(), BackgroundTaskBroadcastReceiver.class)
                                .putExtra(BackgroundTaskSchedulerDelegate.BACKGROUND_TASK_ID_KEY,
                                        TaskIds.TEST);

        BackgroundTaskBroadcastReceiver receiver = new BackgroundTaskBroadcastReceiver();
        receiver.onReceive(ContextUtils.getApplicationContext(), intent);

        Assert.assertTrue(mScheduleLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
        assertEquals(0, mStopped);
        assertEquals(0, mRescheduled);
    }
}
