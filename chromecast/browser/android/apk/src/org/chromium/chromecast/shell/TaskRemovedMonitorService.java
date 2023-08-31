// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

/**
 * Service started and stopped by CastWebContentsActivity responsible for stopping the
 * cast session if the Activity task is removed by the OS.
 */
public class TaskRemovedMonitorService extends Service {
    private static final String TAG = "TaskRemovedMonitor";

    @VisibleForTesting
    static final String ROOT_SESSION_KEY = "rootSessionId";
    @VisibleForTesting
    static final String SESSION_KEY = "sessionId";

    public String mRootSessionId = "";
    public String mSessionId = "";

    public static void start(String rootSessionId, String sessionId) {
        Context ctx = ContextUtils.getApplicationContext();
        Intent serviceIntent = new Intent(ctx, TaskRemovedMonitorService.class);
        serviceIntent.putExtra(ROOT_SESSION_KEY, rootSessionId);
        serviceIntent.putExtra(SESSION_KEY, sessionId);
        ctx.startService(serviceIntent);
    }

    public static void stop() {
        Context ctx = ContextUtils.getApplicationContext();
        Intent serviceIntent = new Intent(ctx, TaskRemovedMonitorService.class);
        ctx.stopService(serviceIntent);
    }

    @Override
    public int onStartCommand(Intent intent, int flag, int startId) {
        mRootSessionId = intent.getStringExtra(ROOT_SESSION_KEY);
        mSessionId = intent.getStringExtra(SESSION_KEY);
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        if (!CastWebContentsActivity.class.getName().equals(
                    rootIntent.getComponent().getClassName())) {
            // Ignore tasks being removed for any other application Activities.
            return;
        }
        if (!mSessionId.isEmpty()
                && mRootSessionId.equals(CastWebContentsIntentUtils.getSessionId(rootIntent))) {
            Log.d(TAG,
                    "Detected CastWebContentsActivity task removed, stopping session: "
                            + mSessionId);
            CastWebContentsComponent.onComponentClosed(mSessionId);
            stopSelf();
        }
    }
}
