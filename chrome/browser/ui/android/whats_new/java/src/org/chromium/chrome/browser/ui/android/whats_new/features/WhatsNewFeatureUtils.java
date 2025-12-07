// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.whats_new.features;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class is responsible for keeping track of features added to What's New, and map the type
 * enum to the corresponding {@link WhatsNewFeature}.
 */
@NullMarked
public class WhatsNewFeatureUtils {
    // Types for all features added to What's New.
    // TODO(crbug.com/390190615): Add instruction about how to update this list.
    @IntDef({
        WhatsNewType.EXAMPLE_FEATURE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface WhatsNewType {
        int EXAMPLE_FEATURE = 0;
        int COUNT = 1;
    }

    /**
     * An array of {@link WhatsNewType} for features to be listed in the the current release of
     * What's New page.
     */
    static final int[] CURRENT_WHATS_NEW_ENTRIES = {};

    @Nullable
    static WhatsNewFeature featureFromType(@WhatsNewType int type) {
        return null;
    }
}
