// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Java implementations of SystemTimeChangeNotifierAndroid functionality.
 * Forwards TIME_SET intent to native SystemTimeChangeNotifierAndroid.
 */
@JNINamespace("chromecast")
public final class SystemTimeChangeNotifierAndroid {
    private BroadcastReceiver mTimeChangeObserver;

    @CalledByNative
    private static SystemTimeChangeNotifierAndroid create() {
        return new SystemTimeChangeNotifierAndroid();
    }

    private SystemTimeChangeNotifierAndroid() {}

    @CalledByNative
    private void initializeFromNative(final long nativeSystemTimeChangeNotifier) {
        // Listen to TIME_SET intent.
        mTimeChangeObserver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                SystemTimeChangeNotifierAndroidJni.get().onTimeChanged(
                        nativeSystemTimeChangeNotifier, SystemTimeChangeNotifierAndroid.this);
            }
        };
        IntentFilter filter = new IntentFilter(Intent.ACTION_TIME_CHANGED);
        ContextUtils.getApplicationContext().registerReceiver(mTimeChangeObserver, filter);
    }

    @CalledByNative private void finalizeFromNative() {
        ContextUtils.getApplicationContext().unregisterReceiver(mTimeChangeObserver);
        mTimeChangeObserver = null;
    }

    @NativeMethods
    interface Natives {
        void onTimeChanged(
                long nativeSystemTimeChangeNotifierAndroid, SystemTimeChangeNotifierAndroid caller);
    }
}
