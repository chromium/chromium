// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.containment;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** An interface for container that should have custom background styling applied to them. */
@NullMarked
public interface ContainmentItem {
    int DEFAULT_MARGIN = -1;
    int DEFAULT_COLOR = -1;
    float DEFAULT_RADIUS = -1f;

    // LINT.IfChange
    @IntDef({BackgroundStyle.CARD, BackgroundStyle.NONE, BackgroundStyle.STANDARD})
    @Retention(RetentionPolicy.SOURCE)
    @interface BackgroundStyle {
        int STANDARD = 0;
        int CARD = 1;
        int NONE = 2;
    }

    // LINT.ThenChange(//components/browser_ui/widget/android/java/res/values/attrs.xml)

    /**
     * Returns the custom background style for the container. By default, the standard background
     * will be used.
     */
    @BackgroundStyle
    default int getCustomBackgroundStyle() {
        return BackgroundStyle.STANDARD;
    }

    /**
     * @return The custom top margin for the container in pixels. If DEFAULT_MARGIN, the default
     *     margin will be used.
     */
    default int getCustomTopMargin() {
        return DEFAULT_MARGIN;
    }

    /**
     * @return The custom bottom margin for the container in pixels. If DEFAULT_MARGIN, the default
     *     margin will be used.
     */
    default int getCustomBottomMargin() {
        return DEFAULT_MARGIN;
    }

    /**
     * @return The custom horizontal margin for the container in pixels. If DEFAULT_MARGIN, the
     *     default margin will be used.
     */
    default int getCustomHorizontalMargin() {
        return DEFAULT_MARGIN;
    }

    /**
     * @return The custom background color for the container. If DEFAULT_COLOR, the default color
     *     will be used.
     */
    default int getCustomBackgroundColor() {
        return DEFAULT_COLOR;
    }
}
