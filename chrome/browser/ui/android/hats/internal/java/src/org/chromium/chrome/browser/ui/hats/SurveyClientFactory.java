// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Factory class used to create SurveyClient.
 */
public class SurveyClientFactory {
    private static SurveyClientFactory sInstance;
    private static boolean sHasInstanceForTesting;

    private final ObservableSupplier<Boolean> mCrashUploadPermissionSupplier;

    protected SurveyClientFactory(ObservableSupplier<Boolean> crashUploadPermissionSupplier) {
        mCrashUploadPermissionSupplier = crashUploadPermissionSupplier != null
                ? crashUploadPermissionSupplier
                : new ObservableSupplierImpl<>();
    }

    /**
     * Initialize the survey factory instance.
     * @param crashUploadPermissionSupplier Supplier for UMA upload permission.
     */
    public static void initialize(ObservableSupplier<Boolean> crashUploadPermissionSupplier) {
        if (sHasInstanceForTesting) return;

        assert sInstance == null : "Instance is already #initialized.";
        sInstance = new SurveyClientFactory(crashUploadPermissionSupplier);
        SurveyMetadata.initializeInBackground();
        ResettersForTesting.register(() -> sInstance = null);
    }

    /**
     * Set a stubbed factory for testing.
     */
    public static void setInstanceForTesting(SurveyClientFactory testFactory) {
        var origin = sInstance;
        sInstance = testFactory;
        sHasInstanceForTesting = (testFactory != null);
        ResettersForTesting.register(() -> {
            sInstance = origin;
            sHasInstanceForTesting = false;
        });
    }

    /**
     * Return the singleton SurveyClientFactory instance.
     */
    public static SurveyClientFactory getInstance() {
        assert sInstance != null : "SurveyClientFactory is not initialized.";
        return sInstance;
    }

    /**
     * Create a new survey client with the given config and ui delegate. Return Null if the input
     * config is not valid.
     *
     * @param config {@link SurveyConfig#get(String)}
     * @param uiDelegate Ui delegate responsible to show survey.
     * @param profile the user's browser profile.
     * @return SurveyClient to display the given survey matching the config.
     */
    public @Nullable SurveyClient createClient(
            @NonNull SurveyConfig config, @NonNull SurveyUiDelegate uiDelegate, Profile profile) {
        if (config.mProbability == 0f || TextUtils.isEmpty(config.mTriggerId)) return null;

        return new SurveyClientImpl(
                config,
                uiDelegate,
                SurveyControllerProvider.create(profile),
                mCrashUploadPermissionSupplier,
                profile);
    }

    /**
     * Get the crash upload supplier initialized in this factory.
     */
    public Supplier<Boolean> getCrashUploadPermissionSupplier() {
        return mCrashUploadPermissionSupplier;
    }
}
