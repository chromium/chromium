// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.externalauth;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Binder;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.components.embedder_support.util.Origin;

/**
 * Utility class for external authentication tools.
 *
 * This class is safe to use on any thread.
 */
public class ExternalAuthUtils {
    public static final int FLAG_SHOULD_BE_GOOGLE_SIGNED = 1 << 0;
    public static final int FLAG_SHOULD_BE_SYSTEM = 1 << 1;
    private static final String TAG = "ExternalAuthUtils";
    private static ExternalAuthUtils sInstance = new ExternalAuthUtils();

    private final ExternalAuthGoogleDelegate mGoogleDelegate;

    public ExternalAuthUtils() {
        mGoogleDelegate = new ExternalAuthGoogleDelegateImpl();
    }

    /**
     * @return The singleton instance of ExternalAuthUtils.
     */
    public static ExternalAuthUtils getInstance() {
        return sInstance;
    }

    /**
     * Gets the calling package names for the current transaction.
     * @param context The context to use for accessing the package manager.
     * @return The calling package names.
     */
    private static String[] getCallingPackages(Context context) {
        int callingUid = Binder.getCallingUid();
        PackageManager pm = context.getApplicationContext().getPackageManager();
        return pm.getPackagesForUid(callingUid);
    }

    /**
     * Returns whether the caller application is a part of the system build.
     * @param pm Package manager to use for getting package related info.
     * @param packageName The package name to inquire about.
     */
    @VisibleForTesting
    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint("WrongConstant")
    public boolean isSystemBuild(PackageManager pm, String packageName) {
        try {
            ApplicationInfo info = pm.getApplicationInfo(packageName, ApplicationInfo.FLAG_SYSTEM);
            if ((info.flags & ApplicationInfo.FLAG_SYSTEM) == 0) throw new SecurityException();
        } catch (NameNotFoundException e) {
            Log.e(TAG, "Package with name " + packageName + " not found");
            return false;
        } catch (SecurityException e) {
            Log.e(TAG, "Caller with package name " + packageName + " is not in the system build");
            return false;
        }

        return true;
    }

    /**
     * Returns whether the current build of Chrome is a Google-signed package.
     * @return whether the currently running application is signed with Google keys.
     */
    public boolean isChromeGoogleSigned() {
        String packageName = ContextUtils.getApplicationContext().getPackageName();
        return isGoogleSigned(packageName);
    }

    /**
     * Returns whether the call is originating from a Google-signed package.
     * @param packageName The package name to inquire about.
     */
    public boolean isGoogleSigned(String packageName) {
        return mGoogleDelegate.isGoogleSigned(packageName);
    }

    /**
     * Returns whether the package can bypass TWA verification.
     * @param packageName The package name to inquire about.
     * @param origin The origin of the TWA.
     */
    public boolean isAllowlistedForTwaVerification(String packageName, Origin origin) {
        return false;
    }

    /**
     * Returns whether the callers of the current transaction contains a package that matches
     * the give authentication requirements.
     * @param context The context to use for getting package information.
     * @param authRequirements The requirements to be exercised on the caller.
     * @param packageToMatch The package name to compare with the caller.
     * @return Whether the caller meets the authentication requirements.
     */
    private boolean isCallerValid(Context context, int authRequirements, String packageToMatch) {
        boolean shouldBeGoogleSigned = (authRequirements & FLAG_SHOULD_BE_GOOGLE_SIGNED) != 0;
        boolean shouldBeSystem = (authRequirements & FLAG_SHOULD_BE_SYSTEM) != 0;

        String[] callingPackages = getCallingPackages(context);
        PackageManager pm = context.getApplicationContext().getPackageManager();
        boolean matchFound = false;

        for (String packageName : callingPackages) {
            if (!TextUtils.isEmpty(packageToMatch) && !packageName.equals(packageToMatch)) continue;
            matchFound = true;
            if ((shouldBeGoogleSigned && !isGoogleSigned(packageName))
                    || (shouldBeSystem && !isSystemBuild(pm, packageName))) {
                return false;
            }
        }
        return matchFound;
    }

    /**
     * Returns whether the callers of the current transaction contains a package that matches
     * the give authentication requirements.
     * @param context The context to use for getting package information.
     * @param authRequirements The requirements to be exercised on the caller.
     * @param packageToMatch The package name to compare with the caller. Should be non-empty.
     * @return Whether the caller meets the authentication requirements.
     */
    public boolean isCallerValidForPackage(
            Context context, int authRequirements, String packageToMatch) {
        assert !TextUtils.isEmpty(packageToMatch);

        return isCallerValid(context, authRequirements, packageToMatch);
    }

    /**
     * Returns whether the callers of the current transaction matches the given authentication
     * requirements.
     * @param context The context to use for getting package information.
     * @param authRequirements The requirements to be exercised on the caller.
     * @return Whether the caller meets the authentication requirements.
     */
    public boolean isCallerValid(Context context, int authRequirements) {
        return isCallerValid(context, authRequirements, "");
    }

    /**
     * Sets an instance for testing.
     * @param externalAuthUtils The instance to set for testing.
     */
    public static void setInstanceForTesting(ExternalAuthUtils externalAuthUtils) {
        sInstance = externalAuthUtils;
    }
}
