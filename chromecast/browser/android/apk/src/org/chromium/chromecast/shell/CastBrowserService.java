// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.os.Process;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

/**
 * This Service allows launching the Cast browser module through an Intent. When the service is
 * created, the browser main loop will start. This allows launching the browser module separately
 * from the base module, so that the memory overhead of the browser is only incurred when needed.
 *
 * <p>The owner must use Context.startService\stopService APIs to start the browser process. This
 * bindService should not be used as it binds the browser service lifetime to the cast service one,
 * hence resulting in more memory usage in idle.
 */
public class CastBrowserService extends Service {
    private static final String TAG = "CastBrowserService";

    private Binder mBinder;

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "onBind");
        CastBrowserHelper.initializeBrowserAsync(ContextUtils.getApplicationContext(), intent);
        // Must return a non-null IBinder instance to notify Core of a successful start up via
        // ServiceConnection.onServiceConnected callback.
        if (mBinder == null) {
            mBinder = new Binder();
        }
        return mBinder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.d(TAG, "onUnbind");
        return false;
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "onDestroy");
        super.stopSelf();
        Process.killProcess(Process.myPid());
    }
}
