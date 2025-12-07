// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.enterprise.reporting;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;

/** Task for kicking off a new signals reporting process. */
@NullMarked
public class SignalsReportingBackgroundTask extends NativeBackgroundTask {
    @Override
    protected int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        return StartBeforeNativeResult.LOAD_NATIVE;
    }

    /**
     * Called when a signals reporting process is ready to start. However due to the complexity of
     * report generation/upload pipeline, it's hard to know when a report has been uploaded (via
     * this callback). So the task claims that processing has been finished here, and we hope that
     * the system keeps us alive long enough to actually finish processing.
     */
    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        SignalsReportingSchedulerBridgeJni.get().startReporting();
        callback.taskFinished(false);
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        // Reschedule task if native didn't complete loading.
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.CHROME_SIGNALS_REPORTING_JOB_ID;

        // The method is called when the task was interrupted due to some reason.
        // It is not called when the task finishes successfully. Reschedule so
        // we can attempt it again.
        return true;
    }
}
