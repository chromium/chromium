// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.device_lock;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Helper class for emitting metrics to the Android.Automotive.DeviceLockDialogAction histogram. */
public class DeviceLockDialogMetrics {

    /**
     * Enum for action taken when user is presented with a device lock explainer dialog.
     *
     * CREATE_DEVICE_LOCK_DIALOG_SHOWN corresponds with the following dialog:
     * (DISMISS_CLICKED / CREATE_DEVICE_LOCK_CLICKED or GO_TO_OS_SETTINGS_CLICKED)
     * - https://hsv.googleplex.com/5047303057440768
     * - https://hsv.googleplex.com/5942685559947264
     *
     * EXISTING_DEVICE_LOCK_DIALOG_SHOWN corresponds with the following dialog:
     * (USE_WITHOUT_AN_ACCOUNT_CLICKED / USER_UNDERSTANDS_CLICKED)
     * - https://hsv.googleplex.com/6265591234035712
     */
    @IntDef({
        DeviceLockDialogAction.CREATE_DEVICE_LOCK_DIALOG_SHOWN,
        DeviceLockDialogAction.EXISTING_DEVICE_LOCK_DIALOG_SHOWN,
        DeviceLockDialogAction.CREATE_DEVICE_LOCK_CLICKED,
        DeviceLockDialogAction.USER_UNDERSTANDS_CLICKED,
        DeviceLockDialogAction.GO_TO_OS_SETTINGS_CLICKED,
        DeviceLockDialogAction.USE_WITHOUT_AN_ACCOUNT_CLICKED,
        DeviceLockDialogAction.DISMISS_CLICKED,
        DeviceLockDialogAction.COUNT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DeviceLockDialogAction {

        /**
         * One DIALOG_SHOWN metric (CREATE_DEVICE_LOCK_DIALOG_SHOWN or
         * EXISTING_DEVICE_LOCK_DIALOG_SHOWN) corresponds with either 1 button click or other action
         * to dismiss the dialog (ex: clicking back on automotive toolbar, clicking outside dialog
         * to dismiss, etc)
         */
        int CREATE_DEVICE_LOCK_DIALOG_SHOWN = 0;

        int EXISTING_DEVICE_LOCK_DIALOG_SHOWN = 1;

        int CREATE_DEVICE_LOCK_CLICKED = 2;
        int USER_UNDERSTANDS_CLICKED = 3;
        int GO_TO_OS_SETTINGS_CLICKED = 4;
        int USE_WITHOUT_AN_ACCOUNT_CLICKED = 5;
        int DISMISS_CLICKED = 6;
        int COUNT = 7;
    }

    @VisibleForTesting
    public static final String DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX =
            "Android.Automotive.DeviceLockDialogAction.";

    public static void recordDeviceLockDialogAction(
            @DeviceLockDialogAction int action, @DeviceLockActivityLauncher.Source String source) {
        RecordHistogram.recordEnumeratedHistogram(
                DEVICE_LOCK_DIALOG_ACTION_HISTOGRAM_PREFIX + source,
                action,
                DeviceLockDialogAction.COUNT);
    }
}
