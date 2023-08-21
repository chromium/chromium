// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import androidx.core.app.NotificationChannelCompat;
import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

/**
 * This Service allows launching the Cast browser module through an Intent. When
 * the service is created, the browser main loop will start. This allows
 * launching the browser module separately from the base module, so that the
 * memory overhead of the browser is only incurred when needed.
 *
 * The owner must use Context.startService\stopService APIs to start the browser
 * process. This bindService should not be used as it binds the browser service
 * lifetime to the cast service one, hence resulting in more memory usage in
 * idle.
 */
public class CastBrowserService extends Service {
    private static final String TAG = "CastBrowserService";

    // Provide a unique ID for the notification channel used by the foreground service
    // notification.
    // This can be anything as long as it's unique within the application.
    private static final String UNIQUE_CHANNEL_ID = "org.chromium.chromecast.shell.cast_channel";

    // Title for the app notification. This doesn't have to correspond to the name of the app
    // itself.
    private static final String APP_NOTIFICATION_NAME = "Chrome Web Runtime for Cast";

    // Provide a unique ID for the foreground service notification. This can be anything as long
    // as it's unique within the application.
    private static final int UNIQUE_NOTIFICATION_ID = 0x40;

    private Notification mNotification;

    private static Notification createNotification() {
        // Create notification channel to prevent notification warnings.
        NotificationChannelCompat.Builder builder =
                new NotificationChannelCompat
                        .Builder(UNIQUE_CHANNEL_ID, NotificationManagerCompat.IMPORTANCE_NONE)
                        .setName(APP_NOTIFICATION_NAME);
        NotificationChannelCompat channel = builder.build();

        NotificationManagerCompat notificationManager =
                NotificationManagerCompat.from(ContextUtils.getApplicationContext());
        notificationManager.createNotificationChannel(channel);
        // This notification is not actually shown anywhere, but is required for calls to
        // startForeground.
        return new NotificationCompat
                .Builder(ContextUtils.getApplicationContext(), UNIQUE_CHANNEL_ID)
                .setContentTitle(APP_NOTIFICATION_NAME)
                .setContentText("Ready to Cast.")
                .build();
    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "onBind");
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flag, int startId) {
        Log.d(TAG, "onStartCommand");
        if (mNotification == null) {
            mNotification = createNotification();
        }
        startForeground(UNIQUE_NOTIFICATION_ID, mNotification);
        CastBrowserHelper.initializeBrowser(getApplicationContext(), intent);
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "onDestroy");
        stopSelf();
    }
}
