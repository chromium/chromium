// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.containment;

import static org.chromium.components.browser_ui.widget.containment.ContainmentItem.DEFAULT_COLOR;
import static org.chromium.components.browser_ui.widget.containment.ContainmentItem.DEFAULT_MARGIN;
import static org.chromium.components.browser_ui.widget.containment.ContainmentItem.DEFAULT_RADIUS;

import org.chromium.build.annotations.NullMarked;

/** A class that holds the styling information for a settings container. */
@NullMarked
public class ContainerStyle {
    private final float mTopRadius;
    private final float mBottomRadius;
    private final int mTopMargin;
    private final int mBottomMargin;
    private final int mHorizontalMargin;
    private final int mVerticalPadding;
    private final int mBackgroundColor;

    /** A container with no background. */
    public static final ContainerStyle EMPTY = new Builder().build();

    private ContainerStyle(Builder builder) {
        mTopRadius = builder.mTopRadius;
        mBottomRadius = builder.mBottomRadius;
        mTopMargin = builder.mTopMargin;
        mBottomMargin = builder.mBottomMargin;
        mHorizontalMargin = builder.mHorizontalMargin;
        mVerticalPadding = builder.mVerticalPadding;
        mBackgroundColor = builder.mBackgroundColor;
    }

    /** Builder for creating a {@link ContainerStyle}. */
    public static class Builder {
        private float mTopRadius = DEFAULT_RADIUS;
        private float mBottomRadius = DEFAULT_RADIUS;
        private int mTopMargin = DEFAULT_MARGIN;
        private int mBottomMargin = DEFAULT_MARGIN;
        private int mHorizontalMargin = DEFAULT_MARGIN;
        private int mVerticalPadding = DEFAULT_MARGIN;
        private int mBackgroundColor = DEFAULT_COLOR;

        public Builder setTopRadius(float topRadius) {
            mTopRadius = topRadius;
            return this;
        }

        public Builder setBottomRadius(float bottomRadius) {
            mBottomRadius = bottomRadius;
            return this;
        }

        public Builder setTopMargin(int topMargin) {
            mTopMargin = topMargin;
            return this;
        }

        public Builder setBottomMargin(int bottomMargin) {
            mBottomMargin = bottomMargin;
            return this;
        }

        public Builder setHorizontalMargin(int horizontalMargin) {
            mHorizontalMargin = horizontalMargin;
            return this;
        }

        public Builder setVerticalPadding(int verticalPadding) {
            mVerticalPadding = verticalPadding;
            return this;
        }

        public Builder setBackgroundColor(int backgroundColor) {
            mBackgroundColor = backgroundColor;
            return this;
        }

        public ContainerStyle build() {
            return new ContainerStyle(this);
        }
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

    /**
     * @return The vertical padding in pixels.
     */
    public int getVerticalPadding() {
        return mVerticalPadding;
    }

    /**
     * @return The background color for the container.
     */
    public int getBackgroundColor() {
        return mBackgroundColor;
    }
}
