// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.Set;

/** Unit tests for {@link BackgroundTaskSchedulerUma}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackgroundTaskSchedulerUmaTest {
    @Spy
    private BackgroundTaskSchedulerUma mUmaSpy;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BackgroundTaskSchedulerUma.setInstanceForTesting(mUmaSpy);
        doNothing().when(mUmaSpy).assertNativeIsLoaded();
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testToUmaEnumValueFromTaskId() {
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_TEST,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(TaskIds.TEST));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_OMAHA,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(TaskIds.OMAHA_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_GCM,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(
                        TaskIds.GCM_BACKGROUND_TASK_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_NOTIFICATIONS,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(
                        TaskIds.NOTIFICATION_SERVICE_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_WEBVIEW_MINIDUMP,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(
                        TaskIds.WEBVIEW_MINIDUMP_UPLOADING_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_CHROME_MINIDUMP,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(
                        TaskIds.CHROME_MINIDUMP_UPLOADING_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_OFFLINE_PAGES,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(
                        TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_OFFLINE_PREFETCH,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(
                        TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_DOWNLOAD_SERVICE,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(
                        TaskIds.DOWNLOAD_SERVICE_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_DOWNLOAD_CLEANUP,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(
                        TaskIds.DOWNLOAD_CLEANUP_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_WEBVIEW_VARIATIONS,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(
                        TaskIds.WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_OFFLINE_CONTENT_NOTIFICATION,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(
                        TaskIds.OFFLINE_PAGES_PREFETCH_NOTIFICATION_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_WEBAPK_UPDATE,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(TaskIds.WEBAPK_UPDATE_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_DOWNLOAD_RESUMPTION,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(
                        TaskIds.DOWNLOAD_RESUMPTION_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_FEED_REFRESH,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(TaskIds.FEED_REFRESH_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_COMPONENT_UPDATE,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(
                        TaskIds.COMPONENT_UPDATE_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_EXPLORE_SITES_REFRESH,
                BackgroundTaskSchedulerUma.toUmaEnumValueFromTaskId(
                        TaskIds.EXPLORE_SITES_REFRESH_JOB_ID));
        assertEquals(BackgroundTaskSchedulerUma.BACKGROUND_TASK_COUNT, 17);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testCacheEvent() {
        String eventName = "event";
        int eventValue = 77;
        mUmaSpy.cacheEvent(eventName, eventValue);

        Set<String> cachedUmaEntries = BackgroundTaskSchedulerUma.getCachedUmaEntries(
                ContextUtils.getAppSharedPreferences());
        assertTrue(cachedUmaEntries.contains("event:77:1"));
        assertEquals(1, cachedUmaEntries.size());

        mUmaSpy.cacheEvent(eventName, eventValue);
        mUmaSpy.cacheEvent(eventName, eventValue);

        cachedUmaEntries = BackgroundTaskSchedulerUma.getCachedUmaEntries(
                ContextUtils.getAppSharedPreferences());
        assertTrue(cachedUmaEntries.contains("event:77:3"));
        assertEquals(1, cachedUmaEntries.size());

        int eventValue2 = 50;
        mUmaSpy.cacheEvent(eventName, eventValue2);

        cachedUmaEntries = BackgroundTaskSchedulerUma.getCachedUmaEntries(
                ContextUtils.getAppSharedPreferences());
        assertTrue(cachedUmaEntries.contains("event:77:3"));
        assertTrue(cachedUmaEntries.contains("event:50:1"));
        assertEquals(2, cachedUmaEntries.size());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testFlushStats() {
        doNothing().when(mUmaSpy).recordEnumeratedHistogram(anyString(), anyInt(), anyInt());

        BackgroundTaskSchedulerUma.getInstance().flushStats();
        verify(mUmaSpy, times(0)).recordEnumeratedHistogram(anyString(), anyInt(), anyInt());

        String eventName = "event";
        int eventValue = 77;
        int eventValue2 = 50;
        mUmaSpy.cacheEvent(eventName, eventValue);
        mUmaSpy.cacheEvent(eventName, eventValue);
        mUmaSpy.cacheEvent(eventName, eventValue);
        mUmaSpy.cacheEvent(eventName, eventValue2);

        BackgroundTaskSchedulerUma.getInstance().flushStats();

        verify(mUmaSpy, times(3))
                .recordEnumeratedHistogram(eq(eventName), eq(eventValue),
                        eq(BackgroundTaskSchedulerUma.BACKGROUND_TASK_COUNT));
        verify(mUmaSpy, times(1))
                .recordEnumeratedHistogram(eq(eventName), eq(eventValue2),
                        eq(BackgroundTaskSchedulerUma.BACKGROUND_TASK_COUNT));
        Set<String> cachedUmaEntries = BackgroundTaskSchedulerUma.getCachedUmaEntries(
                ContextUtils.getAppSharedPreferences());
        assertTrue(cachedUmaEntries.isEmpty());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testReportTaskScheduledSuccess() {
        doNothing().when(mUmaSpy).cacheEvent(anyString(), anyInt());
        BackgroundTaskSchedulerUma.getInstance().reportTaskScheduled(TaskIds.TEST, true);
        verify(mUmaSpy, times(1))
                .cacheEvent(eq("Android.BackgroundTaskScheduler.TaskScheduled.Success"),
                        eq(BackgroundTaskSchedulerUma.BACKGROUND_TASK_TEST));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testReportTaskScheduledFailure() {
        doNothing().when(mUmaSpy).cacheEvent(anyString(), anyInt());
        BackgroundTaskSchedulerUma.getInstance().reportTaskScheduled(
                TaskIds.OFFLINE_PAGES_BACKGROUND_JOB_ID, false);
        verify(mUmaSpy, times(1))
                .cacheEvent(eq("Android.BackgroundTaskScheduler.TaskScheduled.Failure"),
                        eq(BackgroundTaskSchedulerUma.BACKGROUND_TASK_OFFLINE_PAGES));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testReportTaskCanceled() {
        doNothing().when(mUmaSpy).cacheEvent(anyString(), anyInt());
        BackgroundTaskSchedulerUma.getInstance().reportTaskCanceled(
                TaskIds.OFFLINE_PAGES_PREFETCH_JOB_ID);
        verify(mUmaSpy, times(1))
                .cacheEvent(eq("Android.BackgroundTaskScheduler.TaskCanceled"),
                        eq(BackgroundTaskSchedulerUma.BACKGROUND_TASK_OFFLINE_PREFETCH));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testReportTaskStarted() {
        doNothing().when(mUmaSpy).cacheEvent(anyString(), anyInt());
        BackgroundTaskSchedulerUma.getInstance().reportTaskStarted(TaskIds.OMAHA_JOB_ID);
        verify(mUmaSpy, times(1))
                .cacheEvent(eq("Android.BackgroundTaskScheduler.TaskStarted"),
                        eq(BackgroundTaskSchedulerUma.BACKGROUND_TASK_OMAHA));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testReportTaskStopped() {
        doNothing().when(mUmaSpy).cacheEvent(anyString(), anyInt());
        BackgroundTaskSchedulerUma.getInstance().reportTaskStopped(
                TaskIds.GCM_BACKGROUND_TASK_JOB_ID);
        verify(mUmaSpy, times(1))
                .cacheEvent(eq("Android.BackgroundTaskScheduler.TaskStopped"),
                        eq(BackgroundTaskSchedulerUma.BACKGROUND_TASK_GCM));
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testReportTaskStartedNative() {
        doNothing().when(mUmaSpy).cacheEvent(anyString(), anyInt());
        BackgroundTaskSchedulerExternalUma.reportTaskStartedNative(TaskIds.DOWNLOAD_SERVICE_JOB_ID);
        verify(mUmaSpy, times(1))
                .cacheEvent(eq("Android.BackgroundTaskScheduler.TaskLoadedNative"),
                        eq(BackgroundTaskSchedulerUma.BACKGROUND_TASK_DOWNLOAD_SERVICE));
    }
}
