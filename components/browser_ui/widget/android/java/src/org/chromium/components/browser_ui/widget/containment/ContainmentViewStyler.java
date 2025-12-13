// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.containment;

import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.List;

/** A utility class for applying styles to views in settings. */
@NullMarked
public class ContainmentViewStyler {

    /**
     * Applies the specified background style to the given view.
     *
     * @param view The view to apply the background to.
     * @param style The {@link ContainerStyle} to apply.
     */
    public static void applyBackgroundStyle(View view, ContainerStyle style) {
        if (style == ContainerStyle.EMPTY) {
            view.setBackground(null);
            return;
        }
        view.setBackground(
                createRoundedDrawable(
                        style.getTopRadius(), style.getBottomRadius(), style.getBackgroundColor()));
    }

    /**
     * Applies the specified margins to the given view.
     *
     * @param view The view to apply the margins to.
     * @param style The {@link ContainerStyle} to apply.
     */
    public static void applyMargins(View view, ContainerStyle style) {
        if (style == ContainerStyle.EMPTY) return;

        ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        layoutParams.setMargins(
                style.getHorizontalMargin(),
                style.getTopMargin(),
                style.getHorizontalMargin(),
                style.getBottomMargin());
        view.setLayoutParams(layoutParams);

        if (style.getMinHeight() != ContainmentItem.DEFAULT_VALUE) {
            view.setMinimumHeight(style.getMinHeight());
        }
    }

    /**
     * Applies the specified padding to the given view.
     *
     * @param view The view to apply the padding to.
     * @param style The {@link ContainerStyle} to apply.
     */
    static void applyPadding(View view, ContainerStyle style) {
        if (style == ContainerStyle.EMPTY) return;

        boolean innerContentPaddingNeutralized = false;
        // Find the inner RelativeLayout and neutralize its vertical padding.
        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                View child = viewGroup.getChildAt(i);
                if (child instanceof android.widget.RelativeLayout) {
                    View titleView = child.findViewById(android.R.id.title);
                    if (titleView != null) {
                        child.setPadding(0, 0, 0, 0);
                        innerContentPaddingNeutralized = true;
                        break;
                    }
                }
            }
        }

        // Apply custom padding to the root view, conditionally.
        if (innerContentPaddingNeutralized) {
            view.setPadding(
                    view.getPaddingLeft(),
                    style.getVerticalPadding(),
                    view.getPaddingRight(),
                    style.getVerticalPadding());
        }
    }

    /**
     * Creates a rounded drawable with the specified top and bottom radii.
     *
     * @param topRadius The radius for the top corners.
     * @param bottomRadius The radius for the bottom corners.
     * @param color The background color of the drawable.
     * @return A new {@link Drawable} with the specified properties.
     */
    public static Drawable createRoundedDrawable(float topRadius, float bottomRadius, int color) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setShape(GradientDrawable.RECTANGLE);
        drawable.setColor(color);
        drawable.setCornerRadii(
                new float[] {
                    topRadius, topRadius,
                    topRadius, topRadius,
                    bottomRadius, bottomRadius,
                    bottomRadius, bottomRadius
                });
        return drawable;
    }

    /**
     * Traverses the view hierarchy of a root view and applies styling to designated views.
     *
     * @param rootView The root view to traverse for styling.
     * @param controller The {@link ContainmentItemController} to use for generating styles.
     */
    static void styleChildViews(View rootView, ContainmentItemController controller) {
        List<View> views = new ArrayList<>();
        recursivelyFindStyledViews(rootView, views);

        if (views.isEmpty()) return;

        ArrayList<ContainerStyle> styles = controller.generateViewStyles(views);

        for (int i = 0; i < views.size(); i++) {
            applyBackgroundStyle(views.get(i), styles.get(i));
            applyMargins(views.get(i), styles.get(i));
            applyPadding(views.get(i), styles.get(i));
        }
    }

    private static void recursivelyFindStyledViews(View current, List<View> list) {
        // We are looking for views that implement ContainmentItem, which are the
        // user-perceptible items in a preference that need to be individually styled.
        if (current instanceof ContainmentItem && current.getVisibility() == View.VISIBLE) {
            list.add(current);
            return;
        }

        if (current instanceof ViewGroup group) {
            for (int i = 0; i < group.getChildCount(); i++) {
                recursivelyFindStyledViews(group.getChildAt(i), list);
            }
        }
    }
}
