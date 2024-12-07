// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.os.Build;

import androidx.annotation.IntDef;
import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Base interface for NofificationManagerProxy that only supports simple functionalities. Remove
 * this once AsyncNofificationManagerProxy is set to default.
 */
public class NotificationProxyUtils {
    private static Boolean sAreNotificationsEnabled;
    private static Boolean sAreNotificationsEnabledForTest;

    /** Defines the notification event */
    @IntDef({
        NotificationEvent.NO_CALLBACK_START,
        NotificationEvent.NO_CALLBACK_SUCCESS,
        NotificationEvent.NO_CALLBACK_FAILED,
        NotificationEvent.HAS_CALLBACK_START,
        NotificationEvent.HAS_CALLBACK_SUCCESS,
        NotificationEvent.HAS_CALLBACK_FAILED,
        NotificationEvent.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface NotificationEvent {
        int NO_CALLBACK_START = 0; /* A notification call without a callback started */
        int NO_CALLBACK_SUCCESS = 1; /* A notification call without a callback succeeded */
        int NO_CALLBACK_FAILED = 2; /* A notification call without a callback failed */
        int HAS_CALLBACK_START = 3; /* A notification call with a callback started */
        int HAS_CALLBACK_SUCCESS = 4; /* A notification call with a callback succeeded */
        int HAS_CALLBACK_FAILED = 5; /* A notification call with a callback failed */
        int COUNT = 6;
    }

    public static void recordNotificationEventHistogram(@NotificationEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Notifications.Android.NotificationEvent", event, NotificationEvent.COUNT);
    }

    /** Returns whether notifications are enabled for the app. */
    public static boolean areNotificationsEnabled() {
        if (sAreNotificationsEnabledForTest != null) {
            return sAreNotificationsEnabledForTest;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                && NotificationFeatureMap.isEnabled(
                        NotificationFeatureMap.CACHE_NOTIIFICATIONS_ENABLED)) {
            if (sAreNotificationsEnabled == null) {
                sAreNotificationsEnabled = getNotificationsEnabled();
            }
            return sAreNotificationsEnabled;
        }
        return getNotificationsEnabled();
    }

    /** Sets whether notifications are enabled for the app. */
    public static void setNotificationEnabled(boolean enabled) {
        sAreNotificationsEnabled = enabled;
    }

    public static void setNotificationEnabledForTest(Boolean enabled) {
        sAreNotificationsEnabledForTest = enabled;
        ResettersForTesting.register(() -> sAreNotificationsEnabledForTest = null);
    }

    private static boolean getNotificationsEnabled() {
        return NotificationManagerCompat.from(ContextUtils.getApplicationContext())
                .areNotificationsEnabled();
    }
}
