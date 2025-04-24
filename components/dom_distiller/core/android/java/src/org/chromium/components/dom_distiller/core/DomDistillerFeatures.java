// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.core;

import org.chromium.base.MutableBooleanParamWithSafeDefault;
import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.base.MutableIntParamWithSafeDefault;
import org.chromium.build.annotations.NullMarked;

/** Utility class for ongoing reader mode features. */
@NullMarked
public class DomDistillerFeatures {
    /** Returns whether reader mode should trigger on mobile friendly pages if it's distillable. */
    public static boolean triggerOnMobileFriendlyPages() {
        return sReaderModeImprovements.isEnabled()
                && sReaderModeImprovementsTriggerOnMobileFriendlyPages.getValue();
    }

    /** Returns whether reader mode should trigger on mobile friendly pages if it's distillable. */
    public static boolean showAlwaysOnEntryPoint() {
        return sReaderModeImprovements.isEnabled()
                && sReaderModeImprovementsAlwaysOnEntryPoint.getValue();
    }

    /** Returns whether a custom timeout for the cpa should be used. */
    public static boolean enableCustomCpaTimeout() {
        return sReaderModeImprovements.isEnabled()
                && sReaderModeImprovementsEnableCustomCpaTimeout.getValue();
    }

    // Feature names -- alphabetical ordering.
    public static final String READER_MODE_AUTO_DISTILL = "ReaderModeAutoDistill";
    public static final String READER_MODE_IMPROVEMENTS = "ReaderModeImprovements";

    // Feature flags -- alphabetical ordering.
    public static final MutableFlagWithSafeDefault sReaderModeAutoDistill =
            newMutableFlagWithSafeDefault(READER_MODE_AUTO_DISTILL, /* defaultValue= */ false);
    public static final MutableFlagWithSafeDefault sReaderModeImprovements =
            newMutableFlagWithSafeDefault(READER_MODE_IMPROVEMENTS, /* defaultValue= */ false);

    // Feature params -- alphabetical ordering.
    public static final MutableBooleanParamWithSafeDefault
            sReaderModeImprovementsTriggerOnMobileFriendlyPages =
                    sReaderModeImprovements.newBooleanParam(
                            "trigger_on_mobile_friendly_pages", false);
    public static final MutableBooleanParamWithSafeDefault
            sReaderModeImprovementsAlwaysOnEntryPoint =
                    sReaderModeImprovements.newBooleanParam("always_on_entry_point", false);
    public static final MutableBooleanParamWithSafeDefault
            sReaderModeImprovementsEnableCustomCpaTimeout =
                    sReaderModeImprovements.newBooleanParam("custom_cpa_timeout_enabled", false);
    public static final MutableIntParamWithSafeDefault sReaderModeImprovementsCustomCpaTimeout =
            sReaderModeImprovements.newIntParam("custom_cpa_timeout", 300);

    // Private functions below:

    private static MutableFlagWithSafeDefault newMutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return DomDistillerFeatureMap.getInstance()
                .mutableFlagWithSafeDefault(featureName, defaultValue);
    }
}
