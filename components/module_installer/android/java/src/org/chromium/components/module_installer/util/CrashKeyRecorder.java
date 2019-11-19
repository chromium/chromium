// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.util;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.text.TextUtils;

import com.google.android.play.core.splitinstall.SplitInstallManager;
import com.google.android.play.core.splitinstall.SplitInstallManagerFactory;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.components.crash.CrashKeyIndex;
import org.chromium.components.crash.CrashKeys;

import java.util.Arrays;
import java.util.Set;
import java.util.TreeSet;

/**
 * CrashKey Recorder for installed modules.
 */
class CrashKeyRecorder {
    public static void updateCrashKeys() {
        Context context = ContextUtils.getApplicationContext();
        CrashKeys ck = CrashKeys.getInstance();

        // Get modules that are fully installed as split APKs (excluding base which is always
        // installed). Tree set to have ordered and, thus, deterministic results.
        Set<String> fullyInstalledModules = new TreeSet<>();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // Split APKs are only supported on Android L+.
            try {
                PackageManager pm = context.getPackageManager();
                PackageInfo packageInfo = pm.getPackageInfo(BuildInfo.getInstance().packageName, 0);
                if (packageInfo.splitNames != null) {
                    fullyInstalledModules.addAll(Arrays.asList(packageInfo.splitNames));
                }
            } catch (NameNotFoundException e) {
                throw new RuntimeException(e);
            }
        }

        // Create temporary split install manager to retrieve both fully installed and emulated
        // modules. Then remove fully installed ones to get emulated ones only. Querying the
        // installed modules can only be done if splitcompat has already been called. Otherwise,
        // emulation of later modules won't work. If splitcompat has not been called no modules
        // are emulated. Therefore, use an empty set in that case.
        Set<String> emulatedModules = new TreeSet<>();
        if (SplitCompatInitializer.isInitialized()) {
            SplitInstallManager manager = SplitInstallManagerFactory.create(context);
            emulatedModules.addAll(manager.getInstalledModules());
            emulatedModules.removeAll(fullyInstalledModules);
        }

        ck.set(CrashKeyIndex.INSTALLED_MODULES, encodeCrashKeyValue(fullyInstalledModules));
        ck.set(CrashKeyIndex.EMULATED_MODULES, encodeCrashKeyValue(emulatedModules));
    }

    private static String encodeCrashKeyValue(Set<String> moduleNames) {
        if (moduleNames.isEmpty()) return "<none>";
        // Values with dots are interpreted as URLs. Some module names have dots in them. Make sure
        // they don't get sanitized.
        return TextUtils.join(",", moduleNames).replace('.', '$');
    }
}
