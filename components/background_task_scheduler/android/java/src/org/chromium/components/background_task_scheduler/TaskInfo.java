// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.Log;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * TaskInfo represents a request to run a specific {@link BackgroundTask} given the required
 * parameters, such as whether a special type of network is available.
 */
public class TaskInfo {
    private static final String TAG = "BkgrdTaskInfo";

    /**
     * Common interface for all types of task information.
     */
    public interface TimingInfo {
        /**
         * Receives a {@link TimingInfoVisitor}, which will perform actions on this object.
         * @param visitor object that will perform actions on this instance.
         */
        void accept(TimingInfoVisitor visitor);
    }

    /**
     * Common interface for actions over TimingInfo implementations.
     *
     * This implements the Visitor design pattern over {@link TimingInfo} objects.
     * For a guide on how to use it, see the `Performing actions over TimingInfo objects` section
     * in //components/background_task_scheduler/README.md.
     */
    public interface TimingInfoVisitor {
        /**
         * Applies actions on a given {@link OneOffInfo}. This affects information regarding
         * timing for a one-off task.
         * @param oneOffInfo object to act on.
         */
        void visit(OneOffInfo oneOffInfo);
        /**
         * Applies actions on a given {@link PeriodicInfo}. This affects information regarding
         * timing for a periodic task.
         * @param periodicInfo object to act on.
         */
        void visit(PeriodicInfo periodicInfo);
        /**
         * Applies actions on a given {@link ExactInfo}. This affects information regarding
         * timing for an exact task.
         * @param exactInfo object to act on.
         */
        void visit(ExactInfo exactInfo);
    }

    /**
     * Specifies information regarding one-off tasks.
     */
    public static class OneOffInfo implements TimingInfo {
        private final long mWindowStartTimeMs;
        private final long mWindowEndTimeMs;
        private final boolean mHasWindowStartTimeConstraint;
        private final boolean mExpiresAfterWindowEndTime;

        private OneOffInfo(Builder builder) {
            mWindowStartTimeMs = builder.mWindowStartTimeMs;
            mWindowEndTimeMs = builder.mWindowEndTimeMs;
            mHasWindowStartTimeConstraint = builder.mHasWindowStartTimeConstraint;
            mExpiresAfterWindowEndTime = builder.mExpiresAfterWindowEndTime;
        }

        /**
         * @return the start of the window that the task can begin executing as a delta in
         * milliseconds from now.
         */
        public long getWindowStartTimeMs() {
            return mWindowStartTimeMs;
        }

        /**
         * @return the end of the window that the task can begin executing as a delta in
         * milliseconds from now.
         */
        public long getWindowEndTimeMs() {
            return mWindowEndTimeMs;
        }

        /**
         * @return whether this one-off task has a window start time constraint.
         */
        public boolean hasWindowStartTimeConstraint() {
            return mHasWindowStartTimeConstraint;
        }

        /**
         * @return whether this one-off task expires after {@link #getWindowEndTimeMs()}
         * False by default.
         */
        public boolean expiresAfterWindowEndTime() {
            return mExpiresAfterWindowEndTime;
        }

        /**
         * Checks if a one-off task expired.
         * @param scheduleTimeMs the time at which the task was scheduled.
         * @param endTimeMs the time at which the task was set to expire.
         * @param currentTimeMs the current time to check for expiration.
         * @return true if the task expired and false otherwise.
         */
        static boolean getExpirationStatus(
                long scheduleTimeMs, long endTimeMs, long currentTimeMs) {
            return currentTimeMs >= scheduleTimeMs + endTimeMs;
        }

        @Override
        public void accept(TimingInfoVisitor visitor) {
            visitor.visit(this);
        }

        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder("{");
            if (hasWindowStartTimeConstraint()) {
                sb.append("windowStartTimeMs: ").append(mWindowStartTimeMs).append(", ");
            }
            sb.append("windowEndTimeMs: ").append(mWindowEndTimeMs).append(", ");
            sb.append("expiresAfterWindowEndTime (+flex): ").append(mExpiresAfterWindowEndTime);
            sb.append("}");
            return sb.toString();
        }

        /**
         * @return a new {@link Builder} object to set the values of the one-off task.
         */
        public static Builder create() {
            return new Builder();
        }

        /**
         * A helper builder to provide a way to build {@link OneOffInfo}.
         *
         * @see #create()
         */
        public static final class Builder {
            private long mWindowStartTimeMs;
            private long mWindowEndTimeMs;
            // By default, a {@link OneOffInfo} doesn't have a set start time. The start time is
            // considered the time of scheduling the task.
            private boolean mHasWindowStartTimeConstraint;
            // By default, a {@link OneOffInfo} doesn't have the expiration feature activated.
            private boolean mExpiresAfterWindowEndTime;

            public Builder setWindowStartTimeMs(long windowStartTimeMs) {
                mWindowStartTimeMs = windowStartTimeMs;
                mHasWindowStartTimeConstraint = true;
                return this;
            }

            public Builder setWindowEndTimeMs(long windowEndTimeMs) {
                mWindowEndTimeMs = windowEndTimeMs;
                return this;
            }

            public Builder setExpiresAfterWindowEndTime(boolean expiresAfterWindowEndTime) {
                mExpiresAfterWindowEndTime = expiresAfterWindowEndTime;
                return this;
            }

            /**
             * Build the {@link OneOffInfo object} specified by this builder.
             *
             * @return the {@link OneOffInfo} object.
             */
            public OneOffInfo build() {
                return new OneOffInfo(this);
            }
        }
    }

    /**
     * Specifies information regarding periodic tasks.
     */
    public static class PeriodicInfo implements TimingInfo {
        private final long mIntervalMs;
        private final long mFlexMs;
        private final boolean mHasFlex;
        private final boolean mExpiresAfterWindowEndTime;

        private PeriodicInfo(PeriodicInfo.Builder builder) {
            mIntervalMs = builder.mIntervalMs;
            mFlexMs = builder.mFlexMs;
            mHasFlex = builder.mHasFlex;
            mExpiresAfterWindowEndTime = builder.mExpiresAfterWindowEndTime;
        }

        /**
         * @return the interval between occurrences of this task in milliseconds.
         */
        public long getIntervalMs() {
            return mIntervalMs;
        }

        /**
         * @return the flex time for this task. The task can execute at any time in a window of flex
         * length at the end of the period. It is reported in milliseconds.
         */
        public long getFlexMs() {
            return mFlexMs;
        }

        /**
         * @return true whether this task has defined a flex time. False otherwise.
         */
        public boolean hasFlex() {
            return mHasFlex;
        }

        /**
         * @return whether this periodic task expires after {@link #getIntervalMs()} +
         * {@link #getFlexMs()}
         * False by default.
         */
        public boolean expiresAfterWindowEndTime() {
            return mExpiresAfterWindowEndTime;
        }

        /**
         * Checks if a periodic task expired.
         * @param scheduleTimeMs the time at which the task was scheduled.
         * @param intervalTimeMs the interval at which the periodic task was scheduled.
         * @param flexTimeMs the flex time of the task, either set by the caller or the default one.
         * @param currentTimeMs the current time to check for expiration.
         * @return true if the task expired and false otherwise.
         */
        static boolean getExpirationStatus(
                long scheduleTimeMs, long intervalTimeMs, long flexTimeMs, long currentTimeMs) {
            // Whether the task is executed during the wanted time window is determined here. The
            // position of the current time in relation to the time window is calculated here.
            // This position is compared with the time window margins.
            // For example, if a task is scheduled at 6am with an interval of 5h and a flex of
            // 5min, the valid starting times in that day are: 10:55am to 11am, 3:55pm to 4pm and
            // 8:55pm to 9pm. For 7pm as the current time, the time in the interval window is 3h.
            // This is not inside a valid starting time, so the task is considered expired.
            // Similarly, for 8:58pm as the current time, the time in the interval window is 4h
            // and 58min, which fits in a valid interval window.
            // In the case of a flex value equal or bigger than the interval value, the task
            // never expires.
            long timeSinceScheduledMs = currentTimeMs - scheduleTimeMs;
            long deltaTimeComparedToWindowMs = timeSinceScheduledMs % intervalTimeMs;
            return deltaTimeComparedToWindowMs < intervalTimeMs - flexTimeMs
                    && flexTimeMs < intervalTimeMs;
        }

        @Override
        public void accept(TimingInfoVisitor visitor) {
            visitor.visit(this);
        }

        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder("{");
            sb.append("intervalMs: ").append(mIntervalMs).append(", ");
            if (mHasFlex) {
                sb.append(", flexMs: ").append(mFlexMs).append(", ");
            }
            sb.append("expiresAfterWindowEndTime (+flex): ").append(mExpiresAfterWindowEndTime);
            sb.append("}");
            return sb.toString();
        }

        /**
         * @return a new {@link OneOffInfo.Builder} object to set the values of the one-off task.
         */
        public static PeriodicInfo.Builder create() {
            return new PeriodicInfo.Builder();
        }

        /**
         * A helper builder to provide a way to build {@link OneOffInfo}.
         *
         * @see #create()
         */
        public static final class Builder {
            private long mIntervalMs;
            private long mFlexMs;
            // By default, a {@link PeriodicInfo} doesn't have a specified flex and the default
            // one will be used in the scheduler.
            private boolean mHasFlex;
            // By default, a {@link PeriodicInfo} doesn't have the expiration feature activated.
            private boolean mExpiresAfterWindowEndTime;

            public Builder setIntervalMs(long intervalMs) {
                mIntervalMs = intervalMs;
                return this;
            }

            public Builder setFlexMs(long flexMs) {
                mFlexMs = flexMs;
                mHasFlex = true;
                return this;
            }

            public Builder setExpiresAfterWindowEndTime(boolean expiresAfterWindowEndTime) {
                mExpiresAfterWindowEndTime = expiresAfterWindowEndTime;
                return this;
            }
            /**
             * Build the {@link PeriodicInfo object} specified by this builder.
             *
             * @return the {@link PeriodicInfo} object.
             */
            public PeriodicInfo build() {
                return new PeriodicInfo(this);
            }
        }
    }

    /**
     * Specifies information regarding exact tasks.
     */
    static class ExactInfo implements TimingInfo {
        private final long mTriggerAtMs;

        private ExactInfo(Builder builder) {
            mTriggerAtMs = builder.mTriggerAtMs;
        }

        long getTriggerAtMs() {
            return mTriggerAtMs;
        }

        @Override
        public void accept(TimingInfoVisitor visitor) {
            visitor.visit(this);
        }

        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder("{");
            sb.append("triggerAtMs: ").append(mTriggerAtMs).append("}");
            return sb.toString();
        }

        /**
         * @return a new {@link Builder} object to set the values of the exact task.
         */
        static Builder create() {
            return new Builder();
        }

        /**
         * A helper builder to provide a way to build {@link ExactInfo}.
         *
         * @see #create()
         */
        static final class Builder {
            private long mTriggerAtMs;

            /**
             * Sets the exact UTC timestamp at which to schedule the exact task.
             * @param triggerAtMs the UTC timestamp at which the task should be started.
             * @return the {@link Builder} for creating the {@link ExactInfo} object.
             */
            Builder setTriggerAtMs(long triggerAtMs) {
                mTriggerAtMs = triggerAtMs;
                return this;
            }

            /**
             * Build the {@link ExactInfo object} specified by this builder.
             *
             * @return the {@link ExactInfo} object.
             */
            ExactInfo build() {
                return new ExactInfo(this);
            }
        }
    }

    @IntDef({NetworkType.NONE, NetworkType.ANY, NetworkType.UNMETERED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface NetworkType {
        /**
         * This task has no requirements for network connectivity. Default.
         *
         * @see NetworkType
         */
        int NONE = 0;
        /**
         * This task requires network connectivity.
         *
         * @see NetworkType
         */
        int ANY = 1;
        /**
         * This task requires network connectivity that is unmetered.
         *
         * @see NetworkType
         */
        int UNMETERED = 2;
    }

    /**
     * The task ID should be unique across all tasks. A list of such unique IDs exists in
     * {@link TaskIds}.
     */
    private final int mTaskId;

    /**
     * The extras to provide to the {@link BackgroundTask} when it is run.
     */
    @NonNull
    private final Bundle mExtras;

    /**
     * The type of network the task requires to run.
     */
    @NetworkType
    private final int mRequiredNetworkType;

    /**
     * Whether the task requires charging to run.
     */
    private final boolean mRequiresCharging;

    /**
     * Whether or not to persist this task across device reboots.
     */
    private final boolean mIsPersisted;

    /**
     * Whether this task should override any preexisting tasks with the same task id.
     */
    private final boolean mUpdateCurrent;

    /**
     * Task information regarding a type of task.
     */
    private final TimingInfo mTimingInfo;

    private TaskInfo(Builder builder) {
        mTaskId = builder.mTaskId;
        mExtras = builder.mExtras == null ? new Bundle() : builder.mExtras;
        mRequiredNetworkType = builder.mRequiredNetworkType;
        mRequiresCharging = builder.mRequiresCharging;
        mIsPersisted = builder.mIsPersisted;
        mUpdateCurrent = builder.mUpdateCurrent;
        mTimingInfo = builder.mTimingInfo;
    }

    /**
     * @return the unique ID of this task.
     */
    public int getTaskId() {
        return mTaskId;
    }

    /**
     * @return the {@link BackgroundTask} class that will be instantiated for this task.
     */
    @NonNull
    public Class<? extends BackgroundTask> getBackgroundTaskClass() {
        BackgroundTask backgroundTask =
                BackgroundTaskSchedulerFactory.getBackgroundTaskFromTaskId(mTaskId);
        if (backgroundTask == null) {
            Log.w(TAG, "Cannot get BackgorundTask class from task id " + mTaskId);
            return null;
        }
        return backgroundTask.getClass();
    }

    /**
     * @return the extras that will be provided to the {@link BackgroundTask}.
     */
    @NonNull
    public Bundle getExtras() {
        return mExtras;
    }

    /**
     * @return the type of network the task requires to run.
     */
    @NetworkType
    public int getRequiredNetworkType() {
        return mRequiredNetworkType;
    }

    /**
     * @return whether the task requires charging to run.
     */
    public boolean requiresCharging() {
        return mRequiresCharging;
    }

    /**
     * @return whether or not to persist this task across device reboots.
     */
    public boolean isPersisted() {
        return mIsPersisted;
    }

    /**
     * @return whether this task should override any preexisting tasks with the same task id.
     */
    public boolean shouldUpdateCurrent() {
        return mUpdateCurrent;
    }

    /**
     * @return Whether or not this task is a periodic task.
     */
    @Deprecated
    public boolean isPeriodic() {
        return mTimingInfo instanceof PeriodicInfo;
    }

    /**
     * This is part of a {@link TaskInfo} iff the task is a one-off task.
     *
     * @return the specific data if it is a one-off tasks and null otherwise.
     */
    @Deprecated
    public OneOffInfo getOneOffInfo() {
        if (mTimingInfo instanceof OneOffInfo) return (OneOffInfo) mTimingInfo;
        return null;
    }

    /**
     * This is part of a {@link TaskInfo} iff the task is a periodic task.
     *
     * @return the specific data that if it is a periodic tasks and null otherwise.
     */
    @Deprecated
    public PeriodicInfo getPeriodicInfo() {
        if (mTimingInfo instanceof PeriodicInfo) return (PeriodicInfo) mTimingInfo;
        return null;
    }

    /**
     * @return the specific data based on the type of task.
     */
    public TimingInfo getTimingInfo() {
        return mTimingInfo;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("{");
        sb.append("taskId: ").append(mTaskId);
        sb.append(", extras: ").append(mExtras);
        sb.append(", requiredNetworkType: ").append(mRequiredNetworkType);
        sb.append(", requiresCharging: ").append(mRequiresCharging);
        sb.append(", isPersisted: ").append(mIsPersisted);
        sb.append(", updateCurrent: ").append(mUpdateCurrent);
        sb.append(", timingInfo: ").append(mTimingInfo);
        sb.append("}");
        return sb.toString();
    }

    /**
     * Creates a task that holds all information necessary to schedule it.
     *
     * @param taskId the unique task ID for this task. Should be listed in {@link TaskIds}.
     * @param timingInfo the task information specific to each type of task.
     * @return the builder which can be used to continue configuration and {@link Builder#build()}.
     * @see TaskIds
     */
    public static Builder createTask(int taskId, TimingInfo timingInfo) {
        return new Builder(taskId).setTimingInfo(timingInfo);
    }

    /**
     * Schedule a one-off task to execute within a deadline. If windowEndTimeMs is 0, the task will
     * run as soon as possible. For executing a task within a time window, see
     * {@link #createOneOffTask(int, Class, long, long)}.
     *
     * @param taskId the unique task ID for this task. Should be listed in {@link TaskIds}.
     * @param backgroundTaskClass the {@link BackgroundTask} class that will be instantiated for
     * this task.
     * @param windowEndTimeMs the end of the window that the task can begin executing as a delta in
     * milliseconds from now. Note that the task begins executing at this point even if the
     * prerequisite conditions are not met.
     * @return the builder which can be used to continue configuration and {@link Builder#build()}.
     * @see TaskIds
     *
     * @deprecated the {@see #createTask(int, Class, TimingInfo)} method should be used instead.
     * This method requires an additional step for the caller: the creation of the specific
     * {@link TimingInfo} object with the wanted properties.
     */
    @Deprecated
    public static Builder createOneOffTask(
            int taskId, Class<? extends BackgroundTask> backgroundTaskClass, long windowEndTimeMs) {
        TimingInfo oneOffInfo = OneOffInfo.create().setWindowEndTimeMs(windowEndTimeMs).build();
        return new Builder(taskId).setTimingInfo(oneOffInfo);
    }

    /**
     * Schedule a one-off task to execute within a time window. For executing a task within a
     * deadline, see {@link #createOneOffTask(int, Class, long)},
     *
     * @param taskId the unique task ID for this task. Should be listed in {@link TaskIds}.
     * @param backgroundTaskClass the {@link BackgroundTask} class that will be instantiated for
     * this task.
     * @param windowStartTimeMs the start of the window that the task can begin executing as a delta
     * in milliseconds from now.
     * @param windowEndTimeMs the end of the window that the task can begin executing as a delta in
     * milliseconds from now. Note that the task begins executing at this point even if the
     * prerequisite conditions are not met.
     * @return the builder which can be used to continue configuration and {@link Builder#build()}.
     * @see TaskIds
     *
     * @deprecated the {@see #createTask(int, Class, TimingInfo)} method should be used instead.
     * This method requires an additional step for the caller: the creation of the specific
     * {@link TimingInfo} object with the wanted properties.
     */
    @Deprecated
    public static Builder createOneOffTask(int taskId,
            Class<? extends BackgroundTask> backgroundTaskClass, long windowStartTimeMs,
            long windowEndTimeMs) {
        TimingInfo oneOffInfo = OneOffInfo.create()
                                        .setWindowStartTimeMs(windowStartTimeMs)
                                        .setWindowEndTimeMs(windowEndTimeMs)
                                        .build();
        return new Builder(taskId).setTimingInfo(oneOffInfo);
    }

    /**
     * Schedule a periodic task that will recur at the specified interval, without the need to
     * be rescheduled. The task will continue to recur until
     * {@link BackgroundTaskScheduler#cancel(Context, int)} is invoked with the task ID from this
     * {@link TaskInfo}.
     * The flex time specifies how close to the end of the interval you are willing to execute.
     * Instead of executing at the exact interval, the task will execute at the interval or up to
     * flex milliseconds before.
     *
     * @param taskId the unique task ID for this task. Should be listed in {@link TaskIds}.
     * @param backgroundTaskClass the {@link BackgroundTask} class that will be instantiated for
     * this task.
     * @param intervalMs the interval between occurrences of this task in milliseconds.
     * @param flexMs the flex time for this task. The task can execute at any time in a window of
     * flex
     * length at the end of the period. It is reported in milliseconds.
     * @return the builder which can be used to continue configuration and {@link Builder#build()}.
     * @see TaskIds
     *
     * @deprecated the {@see #createTask(int, Class, TimingInfo)} method should be used instead.
     * This method requires an additional step for the caller: the creation of the specific
     * {@link TimingInfo} object with the wanted properties.
     */
    @Deprecated
    public static Builder createPeriodicTask(int taskId,
            Class<? extends BackgroundTask> backgroundTaskClass, long intervalMs, long flexMs) {
        TimingInfo periodicInfo =
                PeriodicInfo.create().setIntervalMs(intervalMs).setFlexMs(flexMs).build();
        return new Builder(taskId).setTimingInfo(periodicInfo);
    }

    /**
     * A helper builder to provide a way to build {@link TaskInfo}. To create a {@link Builder}
     * use the createTask method on {@link TaskInfo}.
     *
     * @see @createTask(int, Class, TimingInfo)
     */
    public static final class Builder {
        private final int mTaskId;

        private Bundle mExtras;
        @NetworkType
        private int mRequiredNetworkType;
        private boolean mRequiresCharging;
        private boolean mIsPersisted;
        private boolean mUpdateCurrent;
        private TimingInfo mTimingInfo;

        Builder(int taskId) {
            mTaskId = taskId;
        }

        Builder setTimingInfo(TimingInfo timingInfo) {
            mTimingInfo = timingInfo;
            return this;
        }

        /**
         * Set the optional extra values necessary for this task. Must only ever contain simple
         * values supported by {@link android.os.BaseBundle}. All other values are thrown away.
         * If the extras for this builder are not set, or set to null, the resulting
         * {@link TaskInfo} will have an empty bundle (i.e. not null).
         *
         * @param bundle the bundle of extra values necessary for this task.
         * @return this {@link Builder}.
         */
        public Builder setExtras(Bundle bundle) {
            mExtras = bundle;
            return this;
        }

        /**
         * Set the type of network the task requires to run.
         *
         * @param networkType the {@link NetworkType} required for this task.
         * @return this {@link Builder}.
         */
        public Builder setRequiredNetworkType(@NetworkType int networkType) {
            mRequiredNetworkType = networkType;
            return this;
        }

        /**
         * Set whether the task requires charging to run.
         *
         * @param requiresCharging true if this task requires charging.
         * @return this {@link Builder}.
         */
        public Builder setRequiresCharging(boolean requiresCharging) {
            mRequiresCharging = requiresCharging;
            return this;
        }

        /**
         * Set whether or not to persist this task across device reboots.
         *
         * @param isPersisted true if this task should be persisted across reboots.
         * @return this {@link Builder}.
         */
        public Builder setIsPersisted(boolean isPersisted) {
            mIsPersisted = isPersisted;
            return this;
        }

        /**
         * Set whether this task should override any preexisting tasks with the same task id.
         *
         * @param updateCurrent true if this task should overwrite a currently existing task with
         *                      the same ID, if it exists.
         * @return this {@link Builder}.
         */
        public Builder setUpdateCurrent(boolean updateCurrent) {
            mUpdateCurrent = updateCurrent;
            return this;
        }

        /**
         * Build the {@link TaskInfo object} specified by this builder.
         *
         * @return the {@link TaskInfo} object.
         */
        public TaskInfo build() {
            return new TaskInfo(this);
        }
    }
}
