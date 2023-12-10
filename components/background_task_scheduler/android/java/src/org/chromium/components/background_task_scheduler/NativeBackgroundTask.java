// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.app.Notification;
import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.BrowserStartupController;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Base class implementing {@link BackgroundTask} that adds native initialization, ensuring that
 * tasks are run after Chrome is successfully started.
 */
public abstract class NativeBackgroundTask implements BackgroundTask {
    /** Specifies which action to take following onStartTaskBeforeNativeLoaded. */
    @IntDef({
        StartBeforeNativeResult.LOAD_NATIVE,
        StartBeforeNativeResult.RESCHEDULE,
        StartBeforeNativeResult.DONE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface StartBeforeNativeResult {
        /** Task should continue to load native parts of browser. */
        int LOAD_NATIVE = 0;

        /** Task should request rescheduling, without loading native parts of browser. */
        int RESCHEDULE = 1;

        /** Task should neither load native parts of browser nor reschedule. */
        int DONE = 2;
    }

    /** The id of the task from {@link TaskParameters}. */
    protected int mTaskId;

    /** Indicates that the task has already been stopped. Should only be accessed on UI Thread. */
    private boolean mTaskStopped;

    /**
     * If true, the task runs in Minimal Browser mode. If false, the task runs in Full Browser
     * Mode.
     */
    private boolean mRunningInMinimalBrowserMode;

    /** Loads native and handles initialization. */
    private NativeBackgroundTaskDelegate mDelegate;

    protected NativeBackgroundTask() {}

    /**
     * Sets the delegate. Must be called before any other public method is invoked.
     * @param delegate The delegate to handle native initialization.
     */
    public void setDelegate(NativeBackgroundTaskDelegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public final boolean onStartTask(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        ThreadUtils.assertOnUiThread();
        assert mDelegate != null;

        mTaskId = taskParameters.getTaskId();

        TaskFinishedCallback wrappedCallback =
                new TaskFinishedCallback() {
                    @Override
                    public void taskFinished(boolean needsReschedule) {
                        PostTask.runOrPostTask(
                                TaskTraits.UI_DEFAULT,
                                () -> {
                                    callback.taskFinished(needsReschedule);
                                });
                    }

                    @Override
                    public void setNotification(int notificationId, Notification notification) {
                        PostTask.runOrPostTask(
                                TaskTraits.UI_DEFAULT,
                                () -> callback.setNotification(notificationId, notification));
                    }
                };

        // WrappedCallback will only be called when the work is done or in onStopTask. If the task
        // is short-circuited early (by returning DONE or RESCHEDULE as a StartBeforeNativeResult),
        // the wrappedCallback is not called. Thus task-finished metrics are only recorded if
        // task-started metrics are.
        @StartBeforeNativeResult
        int beforeNativeResult =
                onStartTaskBeforeNativeLoaded(context, taskParameters, wrappedCallback);

        if (beforeNativeResult == StartBeforeNativeResult.DONE) return false;

        if (beforeNativeResult == StartBeforeNativeResult.RESCHEDULE) {
            // Do not pass in wrappedCallback because this is a short-circuit reschedule. For UMA
            // purposes, tasks are started when runWithNative is called and does not consider
            // short-circuit reschedules such as this.
            PostTask.postTask(TaskTraits.UI_DEFAULT, buildRescheduleRunnable(callback));
            return true;
        }

        assert beforeNativeResult == StartBeforeNativeResult.LOAD_NATIVE;
        runWithNative(
                buildStartWithNativeRunnable(context, taskParameters, wrappedCallback),
                buildRescheduleRunnable(wrappedCallback));
        return true;
    }

    @Override
    public final boolean onStopTask(Context context, TaskParameters taskParameters) {
        ThreadUtils.assertOnUiThread();
        assert mDelegate != null;

        mTaskStopped = true;
        if (isNativeLoadedInFullBrowserMode()) {
            return onStopTaskWithNative(context, taskParameters);
        } else {
            return onStopTaskBeforeNativeLoaded(context, taskParameters);
        }
    }

    /**
     * Ensure that native is started before running the task. If native fails to start, the task is
     * going to be rescheduled, by issuing a {@see TaskFinishedCallback} with parameter set to
     * <c>true</c>.
     *
     * @param startWithNativeRunnable A runnable that will execute #onStartTaskWithNative, after the
     *    native is loaded.
     * @param rescheduleRunnable A runnable that will be called to reschedule the task in case
     *    native initialization fails.
     */
    private final void runWithNative(
            final Runnable startWithNativeRunnable, final Runnable rescheduleRunnable) {
        if (isNativeLoadedInFullBrowserMode()) {
            mRunningInMinimalBrowserMode = false;
            PostTask.postTask(TaskTraits.UI_DEFAULT, startWithNativeRunnable);
            return;
        }

        boolean wasInMinimalBrowserMode = isNativeLoadedInMinimalBrowserMode();
        mRunningInMinimalBrowserMode = supportsMinimalBrowser();

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                new Runnable() {
                    @Override
                    public void run() {
                        // If task was stopped before we got here, don't start native
                        // initialization.
                        if (mTaskStopped) return;

                        // Record transitions from No Native to Minimal Browser Mode and from No
                        // Native to Full Browser mode, but not cases in which Minimal Browser
                        // Mode was already started.
                        if (!wasInMinimalBrowserMode) {
                            getUmaReporter().reportTaskStartedNative(mTaskId);
                        }

                        // Start native initialization.
                        mDelegate.initializeNativeAsync(
                                mRunningInMinimalBrowserMode,
                                startWithNativeRunnable,
                                rescheduleRunnable);
                    }
                });
    }

    /**
     * Descendant classes should override this method if they support running in minimal browser
     * mode.
     *
     * @return if the task supports running in minimal browser mode.
     */
    protected boolean supportsMinimalBrowser() {
        return false;
    }

    /**
     * Method that should be implemented in derived classes to provide implementation of {@link
     * BackgroundTask#onStartTask(Context, TaskParameters, TaskFinishedCallback)} run before native
     * is loaded. Task implementing the method may decide to not load native if it hasn't been
     * loaded yet, by returning DONE, meaning no more work is required for the task, or RESCHEDULE,
     * meaning task needs to be immediately rescheduled.
     * This method is guaranteed to be called before {@link #onStartTaskWithNative}.
     */
    @StartBeforeNativeResult
    protected abstract int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback);

    /**
     * Method that should be implemented in derived classes to provide implementation of {@link
     * BackgroundTask#onStartTask(Context, TaskParameters, TaskFinishedCallback)} when native is
     * loaded.
     * This method will not be called unless {@link #onStartTaskBeforeNativeLoaded} returns
     * LOAD_NATIVE.
     */
    protected abstract void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback);

    /** Called by {@link #onStopTask} if native part of browser was not loaded. */
    protected abstract boolean onStopTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters);

    /** Called by {@link #onStopTask} if native part of browser was loaded. */
    protected abstract boolean onStopTaskWithNative(Context context, TaskParameters taskParameters);

    /** Builds a runnable rescheduling task. */
    private Runnable buildRescheduleRunnable(final TaskFinishedCallback callback) {
        return new Runnable() {
            @Override
            public void run() {
                ThreadUtils.assertOnUiThread();
                if (mTaskStopped) return;
                callback.taskFinished(true);
            }
        };
    }

    /** Builds a runnable starting task with native portion. */
    private Runnable buildStartWithNativeRunnable(
            final Context context,
            final TaskParameters taskParameters,
            final TaskFinishedCallback callback) {
        return new Runnable() {
            @Override
            public void run() {
                ThreadUtils.assertOnUiThread();
                if (mTaskStopped) return;
                onStartTaskWithNative(context, taskParameters, callback);
            }
        };
    }

    /** Whether the native part of the browser is loaded in Full Browser Mode. */
    private boolean isNativeLoadedInFullBrowserMode() {
        return getBrowserStartupController().isFullBrowserStarted();
    }

    /** Whether the native part of the browser is loaded in Minimal Browser Mode. */
    private boolean isNativeLoadedInMinimalBrowserMode() {
        return getBrowserStartupController().isRunningInMinimalBrowserMode();
    }

    protected BrowserStartupController getBrowserStartupController() {
        return BrowserStartupController.getInstance();
    }

    private BackgroundTaskSchedulerExternalUma getUmaReporter() {
        return mDelegate.getUmaReporter();
    }
}
