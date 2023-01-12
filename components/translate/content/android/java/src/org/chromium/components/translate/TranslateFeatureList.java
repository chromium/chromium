// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import org.chromium.base.FeatureList;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Exposes translate-specific features to Java since files in org.chromium.components.translate
 * package cannot depend on org.chromium.chrome.browser.flags.ChromeFeatureList.
 */
// TODO(crbug.com/1060097): Remove/update this once a generalized FeatureList exists.
@JNINamespace("translate::android")
public class TranslateFeatureList {
    /** Alphabetical: */
    public static final String CONTENT_LANGUAGES_DISABLE_OBSERVERS_PARAM = "disable_observers";
    public static final String CONTENT_LANGUAGES_IN_LANGUAGE_PICKER =
            "ContentLanguagesInLanguagePicker";

    // Do not instantiate this class.
    private TranslateFeatureList() {}

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in components/translate/content/android/translate_feature_list.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        assert FeatureList.isNativeInitialized();
        return TranslateFeatureListJni.get().isEnabled(featureName);
    }

    /**
     * Returns a field trial param as a boolean for the specified feature.
     *
     * @param featureName The name of the feature.
     * @param paramName The name of the param.
     * @param defaultValue The boolean value to use if the param is not available.
     * @return The parameter value as a boolean. Default value if the feature does not exist or the
     *         specified parameter does not exist or its string value is neither "true" nor "false".
     */
    public static boolean getFieldTrialParamByFeatureAsBoolean(
            String featureName, String paramName, boolean defaultValue) {
        return TranslateFeatureListJni.get().getFieldTrialParamByFeatureAsBoolean(
                featureName, paramName, defaultValue);
    }

    /**
     * The interface implemented by the automatically generated JNI bindings class
     * TranslateFeatureListJni.
     */
    @NativeMethods
    /* package */ interface Natives {
        boolean isEnabled(String featureName);
        boolean getFieldTrialParamByFeatureAsBoolean(
                String featureName, String paramName, boolean defaultValue);
    }
}
