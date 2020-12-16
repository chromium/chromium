// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import org.chromium.base.FeatureList;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

/**
 * Provides an API for querying the status of Page Info features.
 */
// TODO(crbug.com/1060097): Remove/update this once a generalized FeatureList exists.
@JNINamespace("page_info")
@MainDex
public class PageInfoFeatureList {
    public static final String PAGE_INFO_DISCOVERABILITY = "PageInfoDiscoverability";
    public static final String PAGE_INFO_V2 = "PageInfoV2";

    private PageInfoFeatureList() {}

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in
     * //components/page_info/android/page_info_feature_list.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        Boolean testValue = FeatureList.getTestValueForFeature(featureName);
        if (testValue != null) return testValue;
        assert FeatureList.isNativeInitialized();
        return PageInfoFeatureListJni.get().isEnabled(featureName);
    }

    @NativeMethods
    interface Natives {
        boolean isEnabled(String featureName);
    }
}