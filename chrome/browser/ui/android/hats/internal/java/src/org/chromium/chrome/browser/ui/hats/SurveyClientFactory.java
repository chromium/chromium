// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.hats;

import androidx.annotation.NonNull;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

/**
 * Factory class used to create SurveyClient.
 */
public class SurveyClientFactory {
    private static SurveyClientFactory sInstance;

    private final ObservableSupplier<Boolean> mCrashUploadPermissionSupplier;

    private SurveyClientFactory(ObservableSupplier<Boolean> crashUploadPermissionSupplier) {
        mCrashUploadPermissionSupplier = crashUploadPermissionSupplier != null
                ? crashUploadPermissionSupplier
                : new ObservableSupplierImpl<>();
    }

    /**
     * Initialize the survey factory instance.
     * @param crashUploadPermissionSupplier Supplier for UMA upload permission.
     */
    public static void initialize(ObservableSupplier<Boolean> crashUploadPermissionSupplier) {
        assert sInstance == null : "Instance is already #initialized.";
        sInstance = new SurveyClientFactory(crashUploadPermissionSupplier);
        ResettersForTesting.register(() -> sInstance = null);
    }

    /**
     * Return the singleton SurveyClientFactory instance.
     */
    public static SurveyClientFactory getInstance() {
        assert sInstance != null : "SurveyClientFactory is not initialized.";
        return sInstance;
    }

    /**
     * Create a new survey client with the given config and ui delegate.
     * @param config {@link SurveyConfig#get(String)}
     * @param uiDelegate Ui delegate responsible to show survey.
     * @return SurveyClient to display the given survey matching the config.
     */
    public SurveyClient createClient(
            @NonNull SurveyConfig config, @NonNull SurveyUiDelegate uiDelegate) {
        return new SurveyClientImpl(config, uiDelegate, SurveyControllerProvider.create(),
                mCrashUploadPermissionSupplier);
    }
}
