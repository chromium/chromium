// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.enterprise.reporting;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;

/** Unit tests for SignalsReportingBackgroundTask. */
@RunWith(BaseRobolectricTestRunner.class)
public class SignalsReportingBackgroundTaskTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SignalsReportingSchedulerBridge.Natives mMockSchedulerBridgeNatives;

    @Mock private Context mContext;
    @Mock private BackgroundTask.TaskFinishedCallback mTaskFinishedCallback;

    private TaskParameters mTaskParameter;
    private SignalsReportingBackgroundTask mTask;

    @Before
    public void setUp() {
        // Inject the mock native implementation
        SignalsReportingSchedulerBridgeJni.setInstanceForTesting(mMockSchedulerBridgeNatives);

        mTaskParameter = TaskParameters.create(TaskIds.CHROME_SIGNALS_REPORTING_JOB_ID).build();
        mTask = new SignalsReportingBackgroundTask();
    }

    @Test
    public void testOnStartTaskBeforeNativeLoaded() {
        int result =
                mTask.onStartTaskBeforeNativeLoaded(
                        mContext, mTaskParameter, mTaskFinishedCallback);

        assertEquals(NativeBackgroundTask.StartBeforeNativeResult.LOAD_NATIVE, result);
        verify(mTaskFinishedCallback, never()).taskFinished(/* needsReschedule= */ false);
    }

    @Test
    public void testOnStartTaskWithNative() {
        mTask.onStartTaskWithNative(mContext, mTaskParameter, mTaskFinishedCallback);

        verify(mTaskFinishedCallback, times(1)).taskFinished(anyBoolean());
    }

    @Test
    public void testOnStopTaskBeforeNativeLoaded() {
        boolean shouldReschedule = mTask.onStopTaskBeforeNativeLoaded(mContext, mTaskParameter);

        assertTrue(shouldReschedule);
    }

    @Test
    public void testOnStopTaskWithNative() {
        boolean shouldReschedule = mTask.onStopTaskWithNative(mContext, mTaskParameter);

        assertTrue(shouldReschedule);
    }
}
