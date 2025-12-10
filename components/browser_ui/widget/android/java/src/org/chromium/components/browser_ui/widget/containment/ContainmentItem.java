// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.containment;

import android.graphics.Color;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** An interface for container that should have custom background styling applied to them. */
@NullMarked
public interface ContainmentItem {
    int DEFAULT_COLOR = Color.WHITE;
    float DEFAULT_RADIUS = -1f;
    int DEFAULT_VALUE = -1;

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
     * @return The custom minimum height for the container in pixels. If DEFAULT_VALUE the default
     *     height will be used.
     */
    default int getCustomMinHeight() {
        return DEFAULT_VALUE;
    }

    /**
     * @return The custom background color for the container. If DEFAULT_COLOR, the default color
     *     will be used.
     */
    default int getCustomBackgroundColor() {
        return DEFAULT_COLOR;
    }
}
