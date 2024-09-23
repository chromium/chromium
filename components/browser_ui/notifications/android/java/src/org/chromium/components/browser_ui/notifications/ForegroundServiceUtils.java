// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.notifications;

import android.app.ForegroundServiceStartNotAllowedException;
import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.os.Build;

import androidx.core.app.ServiceCompat;
import androidx.core.content.ContextCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;

/**
 * Utility functions that call into Android foreground service related API, and provides
 * compatibility for older Android versions and work around for Android API bugs.
 */
public class ForegroundServiceUtils {
    private static final String TAG = "ForegroundService";

    private ForegroundServiceUtils() {}

    /** Gets the singleton instance of ForegroundServiceUtils. */
    public static ForegroundServiceUtils getInstance() {
        return ForegroundServiceUtils.LazyHolder.sInstance;
    }

    /** Sets a mocked instance for testing. */
    public static void setInstanceForTesting(ForegroundServiceUtils instance) {
        var oldValue = ForegroundServiceUtils.LazyHolder.sInstance;
        ForegroundServiceUtils.LazyHolder.sInstance = instance;
        ResettersForTesting.register(() -> ForegroundServiceUtils.LazyHolder.sInstance = oldValue);
    }

    private static class LazyHolder {
        private static ForegroundServiceUtils sInstance = new ForegroundServiceUtils();
    }

    /**
     * Starts a service from {@code intent} with the expectation that it will make itself a
     * foreground service with {@link android.app.Service#startForeground(int, Notification)}.
     *
     * @param intent The {@link Intent} to fire to start the service.
     */
    public void startForegroundService(Intent intent) {
        ContextCompat.startForegroundService(ContextUtils.getApplicationContext(), intent);
    }

    /**
     * Upgrades a service from background to foreground after calling
     * {@link #startForegroundService(Intent)}.
     * @param service The service to be foreground.
     * @param id The notification id.
     * @param notification The notification attached to the foreground service.
     * @param foregroundServiceType The type of foreground service. Must be a subset of the
     *                              foreground service types defined in AndroidManifest.xml.
     *                              Use 0 if no foregroundServiceType attribute is defined.
     */
    public void startForeground(
            Service service, int id, Notification notification, int foregroundServiceType) {
        // If android fail to build the notification, do nothing.
        if (notification == null) return;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            try {
                service.startForeground(id, notification, foregroundServiceType);
            } catch (ForegroundServiceStartNotAllowedException e) {
                Log.e(TAG, "channelId=%s notificationId=%s", notification.getChannelId(), id, e);
            }
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            service.startForeground(id, notification, foregroundServiceType);
        } else {
            service.startForeground(id, notification);
        }
    }

    /**
     * Stops the foreground service. See {@link ServiceCompat#stopForeground(Service, int)}.
     * @param service The foreground service to stop.
     * @param flags The flags to stop foreground service.
     */
    public void stopForeground(Service service, int flags) {
        // OnePlus devices may throw NullPointerException, see https://crbug.com/992347.
        try {
            ServiceCompat.stopForeground(service, flags);
        } catch (NullPointerException e) {
            Log.e(TAG, "Failed to stop foreground service, ", e);
        }
    }
}
