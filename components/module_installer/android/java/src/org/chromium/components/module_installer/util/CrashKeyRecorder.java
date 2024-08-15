// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.util;

import android.content.pm.PackageInfo;
import android.text.TextUtils;

import org.chromium.base.PackageUtils;
import org.chromium.components.crash.CrashKeyIndex;
import org.chromium.components.crash.CrashKeys;

import java.util.Collections;
import java.util.Set;
import java.util.TreeSet;

/** CrashKey Recorder for installed modules. */
class CrashKeyRecorder {
    public static void updateCrashKeys() {
        // Get modules that are fully installed as split APKs (excluding base which is always
        // installed). Tree set to have ordered and, thus, deterministic results.
        Set<String> installedModules = new TreeSet<>();
        PackageInfo packageInfo = PackageUtils.getApplicationPackageInfo(0);
        if (packageInfo.splitNames != null) {
            Collections.addAll(installedModules, packageInfo.splitNames);
        }

        CrashKeys ck = CrashKeys.getInstance();
        ck.set(CrashKeyIndex.INSTALLED_MODULES, encodeCrashKeyValue(installedModules));
    }

    private static String encodeCrashKeyValue(Set<String> moduleNames) {
        if (moduleNames.isEmpty()) return "<none>";
        // Values with dots are interpreted as URLs. Some module names have dots in them. Make sure
        // they don't get sanitized.
        return TextUtils.join(",", moduleNames).replace('.', '$');
    }
}
