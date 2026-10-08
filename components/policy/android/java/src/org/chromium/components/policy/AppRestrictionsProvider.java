// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import android.content.Context;
import android.content.Intent;
import android.content.RestrictionsManager;
import android.os.Bundle;
import android.os.SystemClock;
import android.os.UserManager;

import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Concrete app restriction provider, that uses the default android mechanism to retrieve the
 * restrictions.
 */
@NullMarked
public class AppRestrictionsProvider extends AbstractAppRestrictionsProvider {
    private static final String TAG = "AppResProvider";

    /**
     * Get the app restriction information from provided restrictions manager.
     *
     * @param restrictionsManager RestrictionsManager service from Android System service.
     * @return The restrictions for the application, or an empty bundle if they are not available.
     */
    public static Bundle getApplicationRestrictionsFromRestrictionsManager(
            RestrictionsManager restrictionsManager) {
        try {
            Bundle bundle = restrictionsManager.getApplicationRestrictions();
            Log.i(TAG, "#getApplicationRestrictionsFromRestrictionsManager() " + bundle);
            return bundle != null ? bundle : new Bundle();
        } catch (SecurityException e) {
            // Android bug may throw SecurityException. See crbug.com/886814.
            Log.i(TAG, "#getApplicationRestrictionsFromRestrictionsManager() " + e.getMessage());
            return new Bundle();
        }
    }

    /**
     * Get the app restriction information from provided user manager, and record some timing
     * metrics on its runtime.
     * @param userManager UserManager service from Android System service
     * @param packageName package name for target application.
     * @return The restrictions for the provided package name, an empty bundle if they are not
     *         available.
     */
    public static Bundle getApplicationRestrictionsFromUserManager(
            UserManager userManager, String packageName) {
        try {
            Bundle bundle = userManager.getApplicationRestrictions(packageName);
            Log.i(TAG, "#getApplicationRestrictionsFromUserManager() " + bundle);
            return bundle;
        } catch (SecurityException e) {
            // Android bug may throw SecurityException. See crbug.com/886814.
            Log.i(TAG, "#getApplicationRestrictionsFromUserManager() " + e.getMessage());
            return new Bundle();
        }
    }

    private final UserManager mUserManager;
    private final @Nullable RestrictionsManager mRestrictionsManager;

    public AppRestrictionsProvider(Context context) {
        super(context);

        mUserManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
        mRestrictionsManager =
                (RestrictionsManager) context.getSystemService(Context.RESTRICTIONS_SERVICE);
    }

    @Override
    protected Bundle getApplicationRestrictions(String packageName) {
        long startTime = SystemClock.elapsedRealtime();
        Bundle bundle;
        boolean useRestrictionsManager =
                PolicyFeatureMap.sUseRestrictionsManagerInAppRestrictionsProvider.isEnabled();
        try (TraceEvent te =
                TraceEvent.scoped("AppRestrictionsProvider.getApplicationRestrictions")) {
            if (!useRestrictionsManager) {
                bundle = getApplicationRestrictionsFromUserManager(mUserManager, packageName);
            } else if (mRestrictionsManager != null) {
                bundle = getApplicationRestrictionsFromRestrictionsManager(mRestrictionsManager);
            } else {
                Log.w(TAG, "RestrictionsManager unavailable, falling back to UserManager");
                bundle = getApplicationRestrictionsFromUserManager(mUserManager, packageName);
            }
        }
        long durationMs = SystemClock.elapsedRealtime() - startTime;
        RecordHistogram.recordTimesHistogram(
                "Enterprise.Policy.AppRestrictionsProviderFetchTime", durationMs);
        if (useRestrictionsManager) {
            RecordHistogram.recordBooleanHistogram(
                    "Enterprise.Policy.AppRestrictionsProviderRestrictionsManagerAvailable",
                    mRestrictionsManager != null);
        }
        return bundle;
    }

    @Override
    protected String getRestrictionChangeIntentAction() {
        return Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED;
    }
}
