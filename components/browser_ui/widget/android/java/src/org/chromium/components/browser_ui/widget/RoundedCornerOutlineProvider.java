// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.graphics.Outline;
import android.view.View;
import android.view.ViewOutlineProvider;

import androidx.annotation.VisibleForTesting;

/**
 * A custom {@link ViewOutlineProvider} that is able to render content with rounded off corners.
 * The instance guarantees that only the actual bounds of the view are rounded,
 * ie. if the content does not stretch into the corners, it won't be rounded.
 * This class can be applied to any view, including containers.
 *
 * Affect background/foreground colors alike, as well as select/focus states etc.
 *
 * To apply:
 *     myView.setOutlineProvider(new RoundedCornerOutlineProvider(r));
 *     myView.setClipToOutline(true);
 */
public class RoundedCornerOutlineProvider extends ViewOutlineProvider {
    /** Radius of each corner. */
    private int mRadius;

    public RoundedCornerOutlineProvider() {
        this(0);
    }

    public RoundedCornerOutlineProvider(int radius) {
        setRadius(radius);
    }

    @Override
    public void getOutline(View view, Outline outline) {
        outline.setRoundRect(view.getPaddingLeft(), view.getPaddingTop(),
                view.getWidth() - view.getPaddingRight(),
                view.getHeight() - view.getPaddingBottom(), mRadius);
    }

    /**
     * Set the rounding radius.
     * @param radius The radius to apply to round rectangle corners.
     */
    public void setRadius(int radius) {
        mRadius = radius;
    }

    /** Returns the radius used to round the view. */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public int getRadiusForTesting() {
        return mRadius;
    }
}
