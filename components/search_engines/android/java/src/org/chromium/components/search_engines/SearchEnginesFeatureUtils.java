// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.search_engines;

import android.text.TextUtils;

/** Helpers to access feature params for {@link SearchEnginesFeatures}. */
public final class SearchEnginesFeatureUtils {

    public static boolean clayBlockingUseFakeBackend() {
        assert SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)
                : "Avoid accessing params on disabled features!";

        String paramValue =
                SearchEnginesFeatures.getFieldTrialParamByFeature(
                        SearchEnginesFeatures.CLAY_BLOCKING, "use_fake_backend");
        return TextUtils.equals(paramValue, "true");
    }

    public static boolean clayBlockingIsDarkLaunch() {
        assert SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING)
                : "Avoid accessing params on disabled features!";

        String paramValue =
                SearchEnginesFeatures.getFieldTrialParamByFeature(
                        SearchEnginesFeatures.CLAY_BLOCKING, "is_dark_launch");
        return TextUtils.equals(paramValue, "true");
    }

    // Do not instantiate this class.
    private SearchEnginesFeatureUtils() {}
}
