// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.containment;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.R;

/** A utility class for containment UI. */
@NullMarked
public class ContainmentUiUtils {
    /**
     * Parses the containment attributes from the given AttributeSet.
     *
     * @param context The context.
     * @param attrs The AttributeSet.
     * @return A simple object containing the parsed attributes.
     */
    public static ContainmentAttributes parseContainmentAttributes(
            Context context, @Nullable AttributeSet attrs) {
        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.ContainmentItem, 0, 0);
        int backgroundStyle =
                a.getInt(
                        R.styleable.ContainmentItem_backgroundStyle,
                        ContainmentItem.BackgroundStyle.STANDARD);
        int backgroundColor =
                a.getColor(
                        R.styleable.ContainmentItem_backgroundColor, ContainmentItem.DEFAULT_COLOR);
        a.recycle();
        return new ContainmentAttributes(backgroundStyle, backgroundColor);
    }

    /** A simple data class to hold the parsed containment attributes. */
    public static class ContainmentAttributes {
        public final int backgroundStyle;
        public final int backgroundColor;

        ContainmentAttributes(int backgroundStyle, int backgroundColor) {
            this.backgroundStyle = backgroundStyle;
            this.backgroundColor = backgroundColor;
        }
    }
}
