// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import static org.chromium.components.browser_ui.settings.CustomStyledPreference.DEFAULT_MARGIN;

import org.chromium.build.annotations.NullMarked;

/** A class that holds the styling information for a preference. */
@NullMarked
public class PreferenceStyle {
    private final float mTopRadius;
    private final float mBottomRadius;
    private final int mTopMargin;
    private final int mBottomMargin;
    private final int mHorizontalMargin;

    /** A style with no background. */
    public static final PreferenceStyle EMPTY =
            new PreferenceStyle(0, 0, DEFAULT_MARGIN, DEFAULT_MARGIN, DEFAULT_MARGIN);

    /**
     * Constructor for PreferenceStyle.
     *
     * @param topRadius The top radius for the background.
     * @param bottomRadius The bottom radius for the background.
     * @param topMargin The top margin in pixels.
     * @param bottomMargin The bottom margin in pixels.
     * @param horizontalMargin The horizontal margin in pixels.
     */
    public PreferenceStyle(
            float topRadius,
            float bottomRadius,
            int topMargin,
            int bottomMargin,
            int horizontalMargin) {
        mTopRadius = topRadius;
        mBottomRadius = bottomRadius;
        mTopMargin = topMargin;
        mBottomMargin = bottomMargin;
        mHorizontalMargin = horizontalMargin;
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
     * @return The top margin in pixels.
     */
    public int getTopMargin() {
        return mTopMargin;
    }

    /**
     * @return The bottom margin in pixels.
     */
    public int getBottomMargin() {
        return mBottomMargin;
    }

    /**
     * @return The horizontal margin in pixels.
     */
    public int getHorizontalMargin() {
        return mHorizontalMargin;
    }
}
