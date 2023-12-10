// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.installedapp;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources;

import org.chromium.base.ContextUtils;

/** A wrapper around the PackageManager that may be overridden for testing. */
public class PackageManagerDelegate {
    /** See {@link PackageManager#getApplicationInfo(String, int)} */
    public ApplicationInfo getApplicationInfo(String packageName, int flags)
            throws NameNotFoundException {
        return ContextUtils.getApplicationContext()
                .getPackageManager()
                .getApplicationInfo(packageName, flags);
    }

    /** See {@link PackageManager#getResourcesForApplication(ApplicationInfo)} */
    public Resources getResourcesForApplication(ApplicationInfo appInfo)
            throws NameNotFoundException {
        return ContextUtils.getApplicationContext()
                .getPackageManager()
                .getResourcesForApplication(appInfo);
    }

    /** See {@link PackageManager#getPackageInfo(String, int)} */
    public PackageInfo getPackageInfo(String packageName, int flags) throws NameNotFoundException {
        return ContextUtils.getApplicationContext()
                .getPackageManager()
                .getPackageInfo(packageName, flags);
    }
}
