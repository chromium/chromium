// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import org.chromium.base.Features;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Provides an API for querying the status of Page Info features.
 *
 * TODO(crbug.com/1060097): generate this file.
 */
@JNINamespace("page_info")
public class PageInfoFeatures extends Features {
    public static final String PAGE_INFO_STORE_INFO_NAME = "PageInfoStoreInfo";

    // This list must be kept in sync with kFeaturesExposedToJava in page_info_features.cc.
    public static final PageInfoFeatures PAGE_INFO_STORE_INFO =
            new PageInfoFeatures(0, PAGE_INFO_STORE_INFO_NAME);

    private final int mOrdinal;

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
