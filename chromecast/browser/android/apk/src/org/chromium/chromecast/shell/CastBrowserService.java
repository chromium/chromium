// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

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

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "onBind");
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flag, int startId) {
        Log.d(TAG, "onStartCommand");
        CastBrowserHelper.initializeBrowser(getApplicationContext(), intent);
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "onDestroy");
        stopSelf();
    }
}
