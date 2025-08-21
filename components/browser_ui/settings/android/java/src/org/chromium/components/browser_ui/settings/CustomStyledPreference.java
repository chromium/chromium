// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** An interface for preferences that should have custom background styling applied to them. */
@NullMarked
public interface CustomStyledPreference {
    int DEFAULT = -1;

    @IntDef({BackgroundStyle.NONE, BackgroundStyle.CARD})
    @Retention(RetentionPolicy.SOURCE)
    @interface BackgroundStyle {
        int NONE = 0;
        int CARD = 1;
    }

    /** Returns the custom background style for the preference. */
    @BackgroundStyle
    int getCustomBackgroundStyle();

    /**
     * @return The custom top margin for the preference in pixels. If DEFAULT, the default margin
     *     will be used.
     */
    default int getCustomTopMargin() {
        return DEFAULT;
    }

    /**
     * @return The custom bottom margin for the preference in pixels. If DEFAULT, the default margin
     *     will be used.
     */
    default int getCustomBottomMargin() {
        return DEFAULT;
    }
}
