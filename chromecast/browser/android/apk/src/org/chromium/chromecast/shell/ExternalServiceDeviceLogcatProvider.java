// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.RemoteException;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;

import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.StringReader;

/**
 * Gets file with system logs for the Assistant Devices and elide PII sensitive info from it.
 *
 * <p>Elided information includes: Emails, IP address, MAC address, URL/domains as well as
 * Javascript console messages.
 */
class ExternalServiceDeviceLogcatProvider extends ElidedLogcatProvider {
    private static final String TAG = "ExternalLogProvider";

    @Override
    protected void getRawLogcat(RawLogcatCallback callback) {
        Log.i(TAG, "Sending bind command");
        Intent intent = new Intent();

        Context appContext = ContextUtils.getApplicationContext();
        boolean foundLogsProvider = false;
        // TODO(sandv): Inject stub of service for testing
        for (String pkg : BuildConfig.DEVICE_LOGS_PROVIDER_PACKAGE.split(",")) {
            intent.setComponent(new ComponentName(pkg, BuildConfig.DEVICE_LOGS_PROVIDER_CLASS));
            if (appContext.getPackageManager().resolveService(intent, 0) != null) {
                foundLogsProvider = true;
                break;
            }
        }

        if (!foundLogsProvider) {
            Log.e(TAG,
                    "Failed to resolve logs provider: " + BuildConfig.DEVICE_LOGS_PROVIDER_PACKAGE
                            + "/" + BuildConfig.DEVICE_LOGS_PROVIDER_CLASS);
            return;
        }

        appContext.bindService(intent, new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                Log.i(TAG, "onServiceConnected for this");
                IDeviceLogsProvider provider = IDeviceLogsProvider.Stub.asInterface(service);

                ServiceConnection conn = this;

                new AsyncTaskRunner(AsyncTask.THREAD_POOL_EXECUTOR).doAsync(() -> {
                    String logsFileName = "";
                    try {
                        // getLogs() currently gives us the filename of the location of the logs
                        logsFileName = provider.getLogs();
                        return new BufferedReader(new FileReader(logsFileName));
                    } catch (FileNotFoundException | RemoteException e) {
                        Log.e(TAG, "Can't get logs", e);
                        return new BufferedReader(new StringReader(""));
                    } finally {
                        appContext.unbindService(conn);
                    }
                }, callback::onLogsDone);
            }
            @Override
            public void onServiceDisconnected(ComponentName name) {
                Log.i(TAG, "onServiceConnected");
            }
        }, Context.BIND_AUTO_CREATE);
        Log.d(TAG, "Sent bind command");
    }
}
