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
    int DEFAULT_MARGIN = -1;
    int DEFAULT_COLOR = -1;
    float DEFAULT_RADIUS = -1f;

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
     * @return The custom top margin for the preference in pixels. If DEFAULT_MARGIN, the default
     *     margin will be used.
     */
    default int getCustomTopMargin() {
        return DEFAULT_MARGIN;
    }

    /**
     * @return The custom bottom margin for the preference in pixels. If DEFAULT_MARGIN, the default
     *     margin will be used.
     */
    default int getCustomBottomMargin() {
        return DEFAULT_MARGIN;
    }

    /**
     * @return The custom horizontal margin for the preference in pixels. If DEFAULT_MARGIN, the
     *     default margin will be used.
     */
    default int getCustomHorizontalMargin() {
        return DEFAULT_MARGIN;
    }

    /**
     * @return The custom background color for the preference. If DEFAULT_COLOR, the default color
     *     will be used.
     */
    default int getCustomBackgroundColor() {
        return DEFAULT_COLOR;
    }
}
