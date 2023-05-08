// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import org.chromium.base.FeatureList;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Provides an API for querying the status of Site Settings features.
 */
// TODO(crbug.com/1060097): Remove/update this once a generalized FeatureList exists.
@JNINamespace("browser_ui")
public class SiteSettingsFeatureList {
    public static final String SITE_DATA_IMPROVEMENTS = "SiteDataImprovements";
    public static final String REQUEST_DESKTOP_SITE_EXCEPTIONS_DOWNGRADE =
            "RequestDesktopSiteExceptionsDowngrade";

    private SiteSettingsFeatureList() {}

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in
     * //components/browser_ui/site_settings/android/site_settings_feature_list.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        Boolean testValue = FeatureList.getTestValueForFeature(featureName);
        if (testValue != null) return testValue;
        assert FeatureList.isNativeInitialized();
        return SiteSettingsFeatureListJni.get().isEnabled(featureName);
    }

    @NativeMethods
    interface Natives {
        boolean isEnabled(String featureName);
    }
}
