// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.core;

import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.build.annotations.NullMarked;

/** Utility class for ongoing reader mode features. */
@NullMarked
public class DomDistillerFeatures {
    // Feature names -- alphabetical ordering.
    public static final String READER_MODE_DELAY_BOTTOM_SHEET_PEEK =
            "ReaderModeDelayBottomSheetPeek";
    public static final String READER_MODE_TOGGLE_LINKS = "ReaderModeToggleLinks";

    // Feature flags -- alphabetical ordering.
    public static final MutableFlagWithSafeDefault sReaderModeDelayBottomSheetPeek =
            newMutableFlagWithSafeDefault(
                    READER_MODE_DELAY_BOTTOM_SHEET_PEEK, /* defaultValue= */ false);
    public static final MutableFlagWithSafeDefault sReaderModeToggleLinks =
            newMutableFlagWithSafeDefault(READER_MODE_TOGGLE_LINKS, /* defaultValue= */ false);

    // Private functions below:

    private static MutableFlagWithSafeDefault newMutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return DomDistillerFeatureMap.getInstance()
                .mutableFlagWithSafeDefault(featureName, defaultValue);
    }
}
