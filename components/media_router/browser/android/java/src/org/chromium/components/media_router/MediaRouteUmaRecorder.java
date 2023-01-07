// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Record statistics on interesting cast events and actions.
 */
public class MediaRouteUmaRecorder {
    /**
     * Record the number of devices shown in the media route chooser dialog. The function is called
     * three seconds after the dialog is created. The delay time is consistent with how the device
     * count is recorded on Chrome desktop.
     *
     * @param count the number of devices shown.
     */
    public static void recordDeviceCountWithDelay(int count) {
        RecordHistogram.recordCount100Histogram("MediaRouter.Ui.Device.Count", count);
    }

    /**
     * UMA histogram values for the fullscreen controls the user could tap.
     * <p>
     * These values are persisted to logs. Entries should not be renumbered and
     * numeric values should never be reused.
     */
    @IntDef({FullScreenControls.RESUME, FullScreenControls.PAUSE, FullScreenControls.SEEK})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FullScreenControls {
        int RESUME = 0;
        int PAUSE = 1;
        int SEEK = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * UMA histogram values for the notification controls the user could tap.
     * <p>
     * These values are persisted to logs. Entries should not be renumbered and
     * numeric values should never be reused.
     */
    @IntDef({CastNotificationControls.RESUME, CastNotificationControls.PAUSE,
            CastNotificationControls.STOP})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CastNotificationControls {
        int RESUME = 0;
        int PAUSE = 1;
        int STOP = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * Record when an action was taken on the cast notification by the user.
     *
     * @param action one of the CAST_NOTIFICATION_CONTROLS* constants defined above.
     */
    public static void recordCastNotificationControlsAction(int action) {
        RecordHistogram.recordEnumeratedHistogram("Cast.Sender.Clank.NotificationControlsAction",
                action, CastNotificationControls.NUM_ENTRIES);
    }

    /**
     * Record when an action was taken on the {@link CafExpandedControllerActivity} by the user.
     *
     * @param action one of the FULLSCREEN_CONTROLS_* constants defined above.
     */
    public static void recordFullscreenControlsAction(int action) {
        RecordHistogram.recordEnumeratedHistogram("Cast.Sender.Clank.FullscreenControlsAction",
                action, FullScreenControls.NUM_ENTRIES);
    }

    /**
     * Record the ratio of the time the media element was detached from the remote playback session
     * to the total duration of the session (as from when the element has been attached till when
     * the session stopped or disconnected). The ratio is rounded to the nearest tenth and a value
     * of 1 means 10%.
     *
     * @param position the current video playback position in milliseconds.
     * @param duration the total length of the video in milliseconds.
     */
    public static void recordRemoteSessionTimeWithoutMediaElementPercentage(
            long position, long duration) {
        RecordHistogram.recordExactLinearHistogram(
                "Cast.Sender.Clank.SessionTimeWithoutMediaElementPercentage",
                getBucketizedPercentage(position, duration), 11);
    }

    /**
     * Record the ratio of the amount of time remaining on the video when the remote playback stops
     * to the total duration of the session. The ratio is rounded to the nearest tenth and a value
     * of 1 means 10%.
     *
     * @param position the current video playback position in milliseconds.
     * @param duration the total length of the video in milliseconds.
     */
    public static void castEndedTimeRemaining(long position, long duration) {
        RecordHistogram.recordExactLinearHistogram("Cast.Sender.Clank.CastTimeRemainingPercentage",
                getBucketizedPercentage(duration - position, duration), 11);
    }

    /**
     * Calculate what percentage |a| is of |b| and bucketize the result into groups of 10. The
     * result is rounded to the nearest tenth and a value of 1 means 10%.
     */
    private static int getBucketizedPercentage(long a, long b) {
        if (b > 0) {
            return ((int) (10 * a / b));
        } else {
            return 10;
        }
    }
}
