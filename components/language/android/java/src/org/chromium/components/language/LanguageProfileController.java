// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.language;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Controller to manage getting language preferences from device. */
public final class LanguageProfileController {
    private static final String TAG = "ULP";
    private static final int TIMEOUT_IN_SECONDS = 60;

    /**
     * Get the preferred languages for user. The list is empty if an error occurs. This method is
     * blocking and must be called on a background thread.
     *
     * @param accountName Account to get profile or null if the default profile should be returned.
     * @return A list of language tags ordered by preference for |accountName|
     */
    public static List<String> getLanguagePreferences(@Nullable String accountName) {
        boolean signedIn = accountName != null;
        ThreadUtils.assertOnBackgroundThread();
        LanguageProfileDelegate delegate =
                ServiceLoaderUtil.maybeCreate(LanguageProfileDelegate.class);
        if (delegate == null) {
            Log.d(TAG, "ULP not available");
            LanguageProfileMetricsLogger.recordInitiationStatus(
                    signedIn, LanguageProfileMetricsLogger.ULPInitiationStatus.NOT_SUPPORTED);
            return Collections.emptyList();
        }
        try {
            List<String> languages =
                    delegate.getLanguagePreferences(accountName, TIMEOUT_IN_SECONDS);
            LanguageProfileMetricsLogger.recordInitiationStatus(
                    signedIn, LanguageProfileMetricsLogger.ULPInitiationStatus.SUCCESS);
            return languages;
        } catch (TimeoutException e) {
            LanguageProfileMetricsLogger.recordInitiationStatus(
                    signedIn, LanguageProfileMetricsLogger.ULPInitiationStatus.TIMED_OUT);
            Log.d(TAG, "ULP getLanguagePreferences timed out");
        } catch (Exception e) {
            LanguageProfileMetricsLogger.recordInitiationStatus(
                    signedIn, LanguageProfileMetricsLogger.ULPInitiationStatus.FAILURE);
            Log.d(TAG, "ULP getLanguagePreferences threw exception:", e);
        }
        return Collections.emptyList();
    }
}
