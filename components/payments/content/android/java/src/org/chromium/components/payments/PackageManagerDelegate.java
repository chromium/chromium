// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.graphics.drawable.Drawable;
import android.os.StrictMode;
import android.os.StrictMode.ThreadPolicy;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.PackageUtils;

import java.util.List;

/** Abstraction of Android's package manager to enable testing. */
public class PackageManagerDelegate {
    /**
     * Checks whether the system has the given feature.
     * @param feature The feature to check.
     * @return Whether the system has the given feature.
     */
    public boolean hasSystemFeature(String feature) {
        return ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature(feature);
    }

    /**
     * Retrieves package information of an installed application.
     *
     * @param packageName The package name of an installed application.
     * @return The package information of the installed application.
     */
    @SuppressLint("PackageManagerGetSignatures")
    public PackageInfo getPackageInfoWithSignatures(String packageName) {
        return PackageUtils.getPackageInfo(packageName, PackageManager.GET_SIGNATURES);
    }

    /**
     * Retrieves package information of an installed application.
     *
     * @param uid The uid of an installed application.
     * @return The package information of the installed application.
     */
    @SuppressLint("PackageManagerGetSignatures")
    public PackageInfo getPackageInfoWithSignatures(int uid) {
        String packageName =
                ContextUtils.getApplicationContext().getPackageManager().getNameForUid(uid);
        if (packageName == null) return null;
        return getPackageInfoWithSignatures(packageName);
    }

    /**
     * Retrieves the list of activities that can respond to the given intent.
     * @param intent The intent to query.
     * @return The list of activities that can respond to the intent.
     */
    public List<ResolveInfo> getActivitiesThatCanRespondToIntent(Intent intent) {
        return PackageManagerUtils.queryIntentActivities(intent, 0);
    }

    /**
     * Retrieves the list of activities that can respond to the given intent. And returns the
     * activites' meta data in ResolveInfo.
     *
     * @param intent The intent to query.
     * @return The list of activities that can respond to the intent.
     */
    public List<ResolveInfo> getActivitiesThatCanRespondToIntentWithMetaData(Intent intent) {
        return PackageManagerUtils.queryIntentActivities(intent, PackageManager.GET_META_DATA);
    }

    /**
     * Retrieves the list of services that can respond to the given intent.
     * @param intent The intent to query.
     * @return The list of services that can respond to the intent.
     */
    public List<ResolveInfo> getServicesThatCanRespondToIntent(Intent intent) {
        ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            return ContextUtils.getApplicationContext()
                    .getPackageManager()
                    .queryIntentServices(intent, 0);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /**
     * Retrieves the label of the app.
     * @param resolveInfo The identifying information for an app.
     * @return The label for this app.
     */
    public CharSequence getAppLabel(ResolveInfo resolveInfo) {
        return resolveInfo.loadLabel(ContextUtils.getApplicationContext().getPackageManager());
    }

    /**
     * Retrieves the icon of the app.
     * @param resolveInfo The identifying information for an app.
     * @return The icon for this app.
     */
    public Drawable getAppIcon(ResolveInfo resolveInfo) {
        return resolveInfo.loadIcon(ContextUtils.getApplicationContext().getPackageManager());
    }

    /**
     * Gets the string array resource of the given application.
     *
     * @param applicationInfo The application info.
     * @param resourceId      The identifier of the string array resource.
     * @return The string array resource, or null if not found.
     */
    @Nullable
    public String[] getStringArrayResourceForApplication(
            ApplicationInfo applicationInfo, int resourceId) {
        Resources resources;
        try {
            resources =
                    ContextUtils.getApplicationContext()
                            .getPackageManager()
                            .getResourcesForApplication(applicationInfo);
        } catch (NameNotFoundException e) {
            return null;
        }
        if (resources == null) return null;
        try {
            return resources.getStringArray(resourceId);
        } catch (NotFoundException e) {
            return null;
        }
    }

    /**
     * Get the package name of a specified package's installer app.
     * @param packageName The package name of the specified package. Not allowed to be null.
     * @return The package name of the installer app.
     */
    @Nullable
    public String getInstallerPackage(String packageName) {
        assert packageName != null;
        return ContextUtils.getApplicationContext()
                .getPackageManager()
                .getInstallerPackageName(packageName);
    }
}
