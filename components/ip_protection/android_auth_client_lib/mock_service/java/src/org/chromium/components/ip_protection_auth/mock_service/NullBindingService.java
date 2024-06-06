// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.ip_protection_auth.mock_service;

import android.app.Service;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;

import androidx.annotation.Nullable;

import org.chromium.base.Log;

/** A service which just returns null bindings. */
public class NullBindingService extends Service {
    private static final String TAG = "NullBindingService";

    @Override
    public void onCreate() {
        Log.i(TAG, "onCreate");
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "onDestroy");
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            throw new UnsupportedOperationException(
                    "API levels < 28 (Pie) do not support null bindings");
        }
        Log.i(TAG, "returning null binding for %s", intent.toString());
        return null;
    }

    /**
     * A service which is gated behind a permission in AndroidManifest.xml
     *
     * <p>Causes a SecurityException when calling bindService without the permission.
     */
    public static class RestrictedService extends NullBindingService {}

    /**
     * A service which is disabled in AndroidManifest.xml
     *
     * <p>Causes bindService to return false.
     */
    public static class DisabledService extends NullBindingService {}
}
