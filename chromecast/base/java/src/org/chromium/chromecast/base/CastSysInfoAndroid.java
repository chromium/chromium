// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.base;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;

import androidx.core.content.ContextCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;

/**
 * Java implementation of CastSysInfoAndroid methods.
 */
@JNINamespace("chromecast")
public final class CastSysInfoAndroid {
    private static final String TAG = "CastSysInfoAndroid";
    private static final String READ_PRIVILEGED_PHONE_STATE_PERMISSION =
            "android.permission.READ_PRIVILEGED_PHONE_STATE";

    @SuppressLint({"HardwareIds", "MissingPermission"})
    @CalledByNative
    public static String getSerialNumber() {
        String serialNumber = Build.SERIAL;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            Context context = ContextUtils.getApplicationContext();
            int permissionCheck = ContextCompat.checkSelfPermission(context, READ_PRIVILEGED_PHONE_STATE_PERMISSION);
            assert permissionCheck == PackageManager.PERMISSION_GRANTED
                    : "Should not be granted READ_PRIVILEGED_PHONE_STATE_PERMISSION";
        }
        if (!Build.UNKNOWN.equals(serialNumber)) return serialNumber;
        return CastSerialGenerator.getGeneratedSerial();
    }

    @SuppressLint("HardwareIds")
    @CalledByNative
    private static String getBoard() {
        return Build.BOARD;
    }
}
