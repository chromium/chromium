// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.core;

import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.build.annotations.NullMarked;

/** Utility class for ongoing reader mode features. */
@NullMarked
public class DomDistillerFeatures {

    /** Returns whether to provide new accessible font options in the bottom sheet. */
    public static boolean shouldShowNewAccessibleFontOptions() {
        return sReaderModeSupportNewFonts.isEnabled();
    }

    // Feature names -- alphabetical ordering.
    public static final String READER_MODE_DELAY_BOTTOM_SHEET_PEEK =
            "ReaderModeDelayBottomSheetPeek";
    public static final String READER_MODE_DISTILL_IN_APP = "ReaderModeDistillInApp";
    public static final String READER_MODE_SUPPORT_NEW_FONTS = "ReaderModeSupportNewFonts";
    public static final String READER_MODE_TOGGLE_LINKS = "ReaderModeToggleLinks";

    // Feature flags -- alphabetical ordering.
    public static final MutableFlagWithSafeDefault sReaderModeDelayBottomSheetPeek =
            newMutableFlagWithSafeDefault(
                    READER_MODE_DELAY_BOTTOM_SHEET_PEEK, /* defaultValue= */ false);
    public static final MutableFlagWithSafeDefault sReaderModeDistillInApp =
            newMutableFlagWithSafeDefault(READER_MODE_DISTILL_IN_APP, /* defaultValue= */ true);
    public static final MutableFlagWithSafeDefault sReaderModeSupportNewFonts =
            newMutableFlagWithSafeDefault(READER_MODE_SUPPORT_NEW_FONTS, /* defaultValue= */ false);
    public static final MutableFlagWithSafeDefault sReaderModeToggleLinks =
            newMutableFlagWithSafeDefault(READER_MODE_TOGGLE_LINKS, /* defaultValue= */ false);

    // Private functions below:

    private static MutableFlagWithSafeDefault newMutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return DomDistillerFeatureMap.getInstance()
                .mutableFlagWithSafeDefault(featureName, defaultValue);
    }
}
