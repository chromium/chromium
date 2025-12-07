// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.os.Build;

import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Base interface for NofificationManagerProxy that only supports simple functionalities. Remove
 * this once AsyncNofificationManagerProxy is set to default.
 */
@NullMarked
public class NotificationProxyUtils {
    private static @Nullable Boolean sAreNotificationsEnabled;
    private static @Nullable Boolean sAreNotificationsEnabledForTest;

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
