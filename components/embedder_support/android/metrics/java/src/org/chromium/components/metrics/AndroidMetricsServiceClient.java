// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.metrics;

import android.content.Context;
import android.content.pm.ApplicationInfo;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;

/** Helps the native AndroidMetricsServiceClient call Android Java APIs over JNI. */
@JNINamespace("metrics")
public class AndroidMetricsServiceClient {
    private static final String PLAY_STORE_PACKAGE_NAME = "com.android.vending";

    private static @InstallerPackageType Integer sInstallerPackageTypeForTesting;

    @CalledByNative
    private static @InstallerPackageType int getInstallerPackageType() {
        ThreadUtils.assertOnUiThread();
        if (sInstallerPackageTypeForTesting != null) {
            return sInstallerPackageTypeForTesting;
        }
        // Only record if it's a system app or it was installed from Play Store.
        Context ctx = ContextUtils.getApplicationContext();
        if ((ctx.getApplicationInfo().flags & ApplicationInfo.FLAG_SYSTEM) != 0) {
            return InstallerPackageType.SYSTEM_APP;
        } else if (PLAY_STORE_PACKAGE_NAME.equals(BuildInfo.getInstance().installerPackageName)) {
            return InstallerPackageType.GOOGLE_PLAY_STORE;
        }
        return InstallerPackageType.OTHER;
    }

    @CalledByNative
    private static String getAppPackageName() {
        // Return this unconditionally; let native code enforce whether or not it's OK to include
        // this in the logs.
        return BuildInfo.getInstance().hostPackageName;
    }

    public static void setInstallerPackageTypeForTesting(@InstallerPackageType int type) {
        ThreadUtils.assertOnUiThread();
        sInstallerPackageTypeForTesting = type;
    }
}
