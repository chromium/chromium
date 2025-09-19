// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.containment.CustomStyledContainer;

import java.util.ArrayList;
import java.util.List;

/** A utility class for applying styles to views in settings. */
@NullMarked
class SettingsViewStyler {

    /**
     * Applies the specified background style to the given view.
     *
     * @param view The view to apply the background to.
     * @param style The {@link SettingsContainerStyle} to apply.
     */
    static void applyBackgroundStyle(View view, SettingsContainerStyle style) {
        if (style == SettingsContainerStyle.EMPTY) {
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
     * @param style The {@link SettingsContainerStyle} to apply.
     */
    static void applyMargins(View view, SettingsContainerStyle style) {
        if (style == SettingsContainerStyle.EMPTY) return;

        ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        layoutParams.setMargins(
                style.getHorizontalMargin(),
                style.getTopMargin(),
                style.getHorizontalMargin(),
                style.getBottomMargin());
        view.setLayoutParams(layoutParams);
    }

    /**
     * Creates a rounded drawable with the specified top and bottom radii.
     *
     * @param topRadius The radius for the top corners.
     * @param bottomRadius The radius for the bottom corners.
     * @param color The background color of the drawable.
     * @return A new {@link Drawable} with the specified properties.
     */
    private static Drawable createRoundedDrawable(float topRadius, float bottomRadius, int color) {
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
     * @param controller The {@link SettingsStylingController} to use for generating styles.
     */
    static void styleChildViews(View rootView, SettingsStylingController controller) {
        List<View> views = new ArrayList<>();
        recursivelyFindStyledViews(rootView, views);

        if (views.isEmpty()) return;

        ArrayList<SettingsContainerStyle> styles = controller.generateViewStyles(views);

        for (int i = 0; i < views.size(); i++) {
            applyBackgroundStyle(views.get(i), styles.get(i));
            applyMargins(views.get(i), styles.get(i));
        }
    }

    private static void recursivelyFindStyledViews(View current, List<View> list) {
        // We are looking for views that implement CustomStyledContainer, which are the
        // user-perceptible items in a preference that need to be individually styled.
        if (current instanceof CustomStyledContainer && current.getVisibility() == View.VISIBLE) {
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
