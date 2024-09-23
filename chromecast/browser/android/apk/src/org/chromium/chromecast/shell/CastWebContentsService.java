// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.IBinder;

import androidx.core.app.NotificationCompat;

import org.chromium.base.Log;
import org.chromium.chromecast.base.Controller;
import org.chromium.chromecast.base.Observable;
import org.chromium.chromecast.base.Observer;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.WebContents;

import java.util.function.Function;

/**
 * Service for "displaying" a WebContents in CastShell.
 * <p>
 * Typically, this class is controlled by CastContentWindowAndroid, which will bind to this
 * service via CastWebContentsComponent.
 */
public class CastWebContentsService extends Service {
    private static final String TAG = "CastWebService";
    private static final boolean DEBUG = true;
    private static final int CAST_NOTIFICATION_ID = 100;
    private static final String NOTIFICATION_CHANNEL_ID =
            "org.chromium.chromecast.shell.CastWebContentsService.channel";

    private final Controller<Intent> mIntentState = new Controller<>();
    private final Observable<WebContents> mWebContentsState =
            mIntentState.map(CastWebContentsIntentUtils::getWebContents);
    // Allows tests to inject a mock MediaSession to test audio focus logic.
    private Function<WebContents, MediaSession> mMediaSessionGetter = MediaSession::fromWebContents;

    {
        // React to web contents by presenting them in a headless view.
        mWebContentsState.subscribe(CastWebContentsScopes.withoutLayout(this));
        mWebContentsState.subscribe(webContents -> {
            Notification notification =
                    new NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID)
                            .setContentTitle(webContents.getTitle())
                            .setContentText("A Google Cast app is running in the background")
                            .setSmallIcon(R.drawable.ic_settings_cast)
                            .build();
            startForeground(CAST_NOTIFICATION_ID, notification);
            return () -> stopForeground(true /*removeNotification*/);
        });
        mWebContentsState
                .map(this::getMediaSession)
                .subscribe(Observer.onOpen(MediaSession::requestSystemAudioFocus));
        // Inform CastContentWindowAndroid we're detaching.
        Observable<String> instanceIdState = mIntentState.map(Intent::getData).map(Uri::getPath);
        instanceIdState.subscribe(Observer.onClose(CastWebContentsComponent::onComponentClosed));

        if (DEBUG) {
            mWebContentsState.subscribe(x -> {
                Log.d(TAG, "show web contents");
                return () -> Log.d(TAG, "detach web contents");
            });
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if (DEBUG) Log.d(TAG, "onCreate");
        CastBrowserHelper.initializeBrowser(getApplicationContext());
        createNotificationChannel();
    }

    @Override
    public IBinder onBind(Intent intent) {
        if (DEBUG) Log.d(TAG, "onBind");
        intent.setExtrasClassLoader(WebContents.class.getClassLoader());
        mIntentState.set(intent);
        return null;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        if (DEBUG) Log.d(TAG, "onUnbind");
        mIntentState.reset();
        return false;
    }

    private MediaSession getMediaSession(WebContents webContents) {
        return mMediaSessionGetter.apply(webContents);
    }

    Observable<WebContents> observeWebContentsStateForTesting() {
        return mWebContentsState;
    }

    void setMediaSessionGetterForTesting(Function<WebContents, MediaSession> getter) {
        mMediaSessionGetter = getter;
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(NOTIFICATION_CHANNEL_ID,
                    "Cast Audio Apps", NotificationManager.IMPORTANCE_NONE);
            NotificationManager notificationManager =
                    (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
            notificationManager.createNotificationChannel(channel);
        }
    }
}
