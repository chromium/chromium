// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static org.chromium.components.browser_ui.settings.CustomStyledPreference.DEFAULT;

import org.chromium.build.annotations.NullMarked;

/** A class that holds the styling information for a preference. */
@NullMarked
class PreferenceStyle {
    private final float mTopRadius;
    private final float mBottomRadius;
    private final int mTopMargin;
    private final int mBottomMargin;

    /** A style with no background. */
    public static final PreferenceStyle EMPTY = new PreferenceStyle(0, 0, DEFAULT, DEFAULT);

    /**
     * Constructor for PreferenceStyle.
     *
     * @param topRadius The top radius for the background.
     * @param bottomRadius The bottom radius for the background.
     * @param topMargin The top margin in pixels. If DEFAULT, the default margin will be used.
     * @param bottomMargin The bottom margin in pixels. If DEFAULT, the default margin will be used.
     */
    public PreferenceStyle(float topRadius, float bottomRadius, int topMargin, int bottomMargin) {
        mTopRadius = topRadius;
        mBottomRadius = bottomRadius;
        mTopMargin = topMargin;
        mBottomMargin = bottomMargin;
    }

    /**
     * @return The top radius for the background.
     */
    public float getTopRadius() {
        return mTopRadius;
    }

    /**
     * @return The bottom radius for the background.
     */
    public float getBottomRadius() {
        return mBottomRadius;
    }

    /**
     * @return The top margin in pixels. If DEFAULT, the default margin should be used.
     */
    public int getTopMargin() {
        return mTopMargin;
    }

    /**
     * @return The bottom margin in pixels. If DEFAULT, the default margin should be used.
     */
    public int getBottomMargin() {
        return mBottomMargin;
    }
}
