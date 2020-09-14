// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.os.UserManager;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Concrete app restriction provider, that uses the default android mechanism to retrieve the
 * restrictions.
 */
public class AppRestrictionsProvider extends AbstractAppRestrictionsProvider {
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
            return userManager.getApplicationRestrictions(packageName);
        } catch (SecurityException e) {
            // Android bug may throw SecurityException. See crbug.com/886814.
            return new Bundle();
        }
    }

    private final UserManager mUserManager;

    public AppRestrictionsProvider(Context context) {
        super(context);

        mUserManager = (UserManager) context.getSystemService(Context.USER_SERVICE);
    }

    @Override
    protected Bundle getApplicationRestrictions(String packageName) {
        long startTime = SystemClock.elapsedRealtime();
        Bundle bundle = getApplicationRestrictionsFromUserManager(mUserManager, packageName);
        long endTime = SystemClock.elapsedRealtime();
        long duration = endTime - startTime;
        RecordHistogram.recordTimesHistogram("Enterprise.AppRestrictionLoadTime2", duration);
        if (bundle.isEmpty()) {
            RecordHistogram.recordTimesHistogram(
                    "Enterprise.AppRestrictionLoadTime2.EmptyBundle", duration);
        } else {
            RecordHistogram.recordTimesHistogram(
                    "Enterprise.AppRestrictionLoadTime2.NonEmptyBundle", duration);
        }
        return bundle;
    }

    @Override
    protected String getRestrictionChangeIntentAction() {
        return Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED;
    }
}
