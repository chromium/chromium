// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskInfo;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link TaskInfo}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TaskInfoTest {
    private static final long TEST_START_MS = TimeUnit.MINUTES.toMillis(5);
    private static final long TEST_END_MS = TimeUnit.MINUTES.toMillis(10);
    private static final long TEST_FLEX_MS = 100;

    @Before
    public void setUp() {
        BackgroundTaskSchedulerFactoryInternal.setBackgroundTaskFactory(
                new TestBackgroundTaskFactory());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testGeneralFields() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowEndTimeMs(TEST_END_MS)
                        .setExpiresAfterWindowEndTime(true)
                        .build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();

        assertEquals(TaskIds.TEST, oneOffTask.getTaskId());
        assertEquals(
                TestBackgroundTask.class,
                BackgroundTaskSchedulerFactoryInternal.getBackgroundTaskFromTaskId(TaskIds.TEST)
                        .getClass());
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffExpirationWithinDeadline() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowEndTimeMs(TEST_END_MS)
                        .setExpiresAfterWindowEndTime(true)
                        .build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        CheckTimingInfoVisitor visitor = new CheckTimingInfoVisitor(null, TEST_END_MS, true);
        oneOffTask.getTimingInfo().accept(visitor);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffExpirationWithinTimeWindow() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowStartTimeMs(TEST_START_MS)
                        .setWindowEndTimeMs(TEST_END_MS)
                        .setExpiresAfterWindowEndTime(true)
                        .build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        CheckTimingInfoVisitor visitor =
                new CheckTimingInfoVisitor(TEST_START_MS, TEST_END_MS, true);
        oneOffTask.getTimingInfo().accept(visitor);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffExpirationWithinZeroTimeWindow() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.OneOffInfo.create()
                        .setWindowStartTimeMs(TEST_END_MS)
                        .setWindowEndTimeMs(TEST_END_MS)
                        .setExpiresAfterWindowEndTime(true)
                        .build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        CheckTimingInfoVisitor visitor = new CheckTimingInfoVisitor(TEST_END_MS, TEST_END_MS, true);
        oneOffTask.getTimingInfo().accept(visitor);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testOneOffNoParamsSet() {
        TaskInfo.TimingInfo timingInfo = TaskInfo.OneOffInfo.create().build();
        TaskInfo oneOffTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        CheckTimingInfoVisitor visitor = new CheckTimingInfoVisitor(null, Long.valueOf(0), false);
        oneOffTask.getTimingInfo().accept(visitor);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicExpirationWithInterval() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.PeriodicInfo.create()
                        .setIntervalMs(TEST_END_MS)
                        .setExpiresAfterWindowEndTime(true)
                        .build();
        TaskInfo periodicTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        CheckTimingInfoVisitor visitor = new CheckTimingInfoVisitor(TEST_END_MS, null, true);
        periodicTask.getTimingInfo().accept(visitor);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicExpirationWithIntervalAndFlex() {
        TaskInfo.TimingInfo timingInfo =
                TaskInfo.PeriodicInfo.create()
                        .setIntervalMs(TEST_END_MS)
                        .setFlexMs(TEST_FLEX_MS)
                        .setExpiresAfterWindowEndTime(true)
                        .build();
        TaskInfo periodicTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        CheckTimingInfoVisitor visitor =
                new CheckTimingInfoVisitor(TEST_END_MS, TEST_FLEX_MS, true);
        periodicTask.getTimingInfo().accept(visitor);
    }

    @Test
    @Feature({"BackgroundTaskScheduler"})
    public void testPeriodicNoParamsSet() {
        TaskInfo.TimingInfo timingInfo = TaskInfo.PeriodicInfo.create().build();
        TaskInfo periodicTask = TaskInfo.createTask(TaskIds.TEST, timingInfo).build();
        CheckTimingInfoVisitor visitor = new CheckTimingInfoVisitor(Long.valueOf(0), null, false);
        periodicTask.getTimingInfo().accept(visitor);
    }

    private static class CheckTimingInfoVisitor implements TaskInfo.TimingInfoVisitor {
        private final Long mStartOrIntervalOrTriggerMs;
        private final Long mEndOrFlexMs;
        private final boolean mExpires;

        CheckTimingInfoVisitor(Long startOrIntervalOrTriggerMs, Long endOrFlexMs, boolean expires) {
            mStartOrIntervalOrTriggerMs = startOrIntervalOrTriggerMs;
            mEndOrFlexMs = endOrFlexMs;
            mExpires = expires;
        }

        @Override
        public void visit(TaskInfo.OneOffInfo oneOffInfo) {
            if (mStartOrIntervalOrTriggerMs == null) {
                assertFalse(oneOffInfo.hasWindowStartTimeConstraint());
            } else {
                assertTrue(oneOffInfo.hasWindowStartTimeConstraint());
                assertEquals(
                        mStartOrIntervalOrTriggerMs.longValue(), oneOffInfo.getWindowStartTimeMs());
            }

            assertEquals(mEndOrFlexMs.longValue(), oneOffInfo.getWindowEndTimeMs());
            assertEquals(mExpires, oneOffInfo.expiresAfterWindowEndTime());
        }

        @Override
        public void visit(TaskInfo.PeriodicInfo periodicInfo) {
            assertEquals(mStartOrIntervalOrTriggerMs.longValue(), periodicInfo.getIntervalMs());

            if (mEndOrFlexMs == null) {
                assertFalse(periodicInfo.hasFlex());
            } else {
                assertTrue(periodicInfo.hasFlex());
                assertEquals(mEndOrFlexMs.longValue(), periodicInfo.getFlexMs());
            }
        }
    }
}
