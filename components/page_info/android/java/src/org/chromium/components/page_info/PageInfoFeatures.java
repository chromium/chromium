// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Features;

/**
 * Provides an API for querying the status of Page Info features.
 *
 * <p>TODO(crbug.com/40121881): generate this file.
 */
@JNINamespace("page_info")
public class PageInfoFeatures extends Features {
    public static final String USER_BYPASS_UI_NAME = "UserBypassUI";

    // This list must be kept in sync with kFeaturesExposedToJava in page_info_features.cc.
    public static final PageInfoFeatures USER_BYPASS_UI =
            new PageInfoFeatures(0, USER_BYPASS_UI_NAME);

    private final int mOrdinal;

    private static final String PARAM_EXPIRATION = "expiration";

    public static String getUserBypassExpiration() {
        return USER_BYPASS_UI.getFieldTrialParamByFeatureAsString(PARAM_EXPIRATION);
    }

    private PageInfoFeatures(int ordinal, String name) {
        super(name);
        mOrdinal = ordinal;
    }

    @Override
    protected long getFeaturePointer() {
        return PageInfoFeaturesJni.get().getFeature(mOrdinal);
    }

    @NativeMethods
    interface Natives {
        long getFeature(int ordinal);
    }
}
