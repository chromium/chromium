// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.util;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Region;
import android.support.v4.graphics.drawable.RoundedBitmapDrawable;
import android.support.v4.graphics.drawable.RoundedBitmapDrawableFactory;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;

/**
 * View-related utility methods.
 */
public class ViewUtils {
    private static final int[] sLocationTmp = new int[2];

    /**
     * Invalidates a view and all of its descendants.
     */
    private static void recursiveInvalidate(View view) {
        view.invalidate();
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            int childCount = group.getChildCount();
            for (int i = 0; i < childCount; i++) {
                View child = group.getChildAt(i);
                if (child.getVisibility() == View.VISIBLE) {
                    recursiveInvalidate(child);
                }
            }
        }
    }

    /**
     * Sets the enabled property of a View and all of its descendants.
     */
    public static void setEnabledRecursive(View view, boolean enabled) {
        view.setEnabled(enabled);
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) {
                setEnabledRecursive(group.getChildAt(i), enabled);
            }
        }
    }

    /**
     * Captures a bitmap of a View and draws it to a Canvas.
     */
    public static void captureBitmap(View view, Canvas canvas) {
        // Invalidate all the descendants of view, before calling view.draw(). Otherwise, some of
        // the descendant views may optimize away their drawing. http://crbug.com/415251
        recursiveInvalidate(view);
        view.draw(canvas);
    }

    /**
     * Return the position of {@code childView} relative to {@code rootView}.  {@code childView}
     * must be a child of {@code rootView}.  This returns the relative layout position, which does
     * not include translations.
     * @param rootView    The parent of {@code childView} to calculate the position relative to.
     * @param childView   The {@link View} to calculate the position of.
     * @param outPosition The resulting position with the format [x, y].
     */
    public static void getRelativeLayoutPosition(View rootView, View childView, int[] outPosition) {
        assert outPosition.length == 2;
        outPosition[0] = 0;
        outPosition[1] = 0;
        if (rootView == null || childView == rootView) return;
        while (childView != null) {
            outPosition[0] += childView.getLeft();
            outPosition[1] += childView.getTop();
            if (childView.getParent() == rootView) break;
            childView = (View) childView.getParent();
        }
    }

    /**
     * Return the position of {@code childView} relative to {@code rootView}.  {@code childView}
     * must be a child of {@code rootView}.  This returns the relative draw position, which includes
     * translations.
     * @param rootView    The parent of {@code childView} to calculate the position relative to.
     * @param childView   The {@link View} to calculate the position of.
     * @param outPosition The resulting position with the format [x, y].
     */
    public static void getRelativeDrawPosition(View rootView, View childView, int[] outPosition) {
        assert outPosition.length == 2;
        outPosition[0] = 0;
        outPosition[1] = 0;
        if (rootView == null || childView == rootView) return;
        while (childView != null) {
            outPosition[0] = (int) (outPosition[0] + childView.getX());
            outPosition[1] = (int) (outPosition[1] + childView.getY());
            if (childView.getParent() == rootView) break;
            childView = (View) childView.getParent();
        }
    }

    /**
     * Helper for overriding {@link ViewGroup#gatherTransparentRegion} for views that are fully
     * opaque and have children extending beyond their bounds. If the transparent region
     * optimization is turned on (which is the case whenever the view hierarchy contains a
     * SurfaceView somewhere), the children might otherwise confuse the SurfaceFlinger.
     */
    public static void gatherTransparentRegionsForOpaqueView(View view, Region region) {
        view.getLocationInWindow(sLocationTmp);
        region.op(sLocationTmp[0], sLocationTmp[1],
                sLocationTmp[0] + view.getRight() - view.getLeft(),
                sLocationTmp[1] + view.getBottom() - view.getTop(), Region.Op.DIFFERENCE);
    }

    /**
     *  Converts density-independent pixels (dp) to pixels on the screen (px).
     *
     *  @param dp Density-independent pixels are based on the physical density of the screen.
     *  @return   The physical pixels on the screen which correspond to this many
     *            density-independent pixels for this screen.
     */
    public static int dpToPx(Context context, float dp) {
        return dpToPx(context.getResources().getDisplayMetrics(), dp);
    }

    /**
     *  Converts density-independent pixels (dp) to pixels on the screen (px).
     *
     *  @param dp Density-independent pixels are based on the physical density of the screen.
     *  @return   The physical pixels on the screen which correspond to this many
     *            density-independent pixels for this screen.
     */
    public static int dpToPx(DisplayMetrics metrics, float dp) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, dp, metrics));
    }

    /**
     * Sets clip children for the provided ViewGroup and all of its ancestors.
     * @param view The ViewGroup whose children should (not) be clipped.
     * @param clip Whether to clip children to the parent bounds.
     */
    public static void setAncestorsShouldClipChildren(ViewGroup view, boolean clip) {
        ViewGroup parent = view;
        while (parent != null) {
            parent.setClipChildren(clip);
            if (!(parent.getParent() instanceof ViewGroup)) break;
            if (parent.getId() == android.R.id.content) break;
            parent = (ViewGroup) parent.getParent();
        }
    }

    /**
     * Creates a {@link RoundedBitmapDrawable} using the provided {@link Bitmap} and cornerRadius.
     * @param resources The {@link Resources}.
     * @param icon The {@link Bitmap} to round.
     * @param cornerRadius The corner radius.
     * @return A {@link RoundedBitmapDrawable} for the provided {@link Bitmap}.
     */
    public static RoundedBitmapDrawable createRoundedBitmapDrawable(
            Resources resources, Bitmap icon, int cornerRadius) {
        RoundedBitmapDrawable roundedIcon = RoundedBitmapDrawableFactory.create(resources, icon);
        roundedIcon.setCornerRadius(cornerRadius);
        return roundedIcon;
    }
}
