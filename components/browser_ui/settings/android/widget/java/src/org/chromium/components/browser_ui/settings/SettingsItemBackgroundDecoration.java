// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;

/**
 * A {@link RecyclerView.ItemDecoration} that applies a specified custom background to the settings
 * items.
 */
@NullMarked
public class SettingsItemBackgroundDecoration extends RecyclerView.ItemDecoration {
    private @Nullable ArrayList<SettingsContainerStyle> mPreferenceStyles;

    /**
     * A flag to ensure that the background update logic in {@link #onDraw(Canvas, RecyclerView,
     * RecyclerView.State)} is not executed on every frame. This is a performance optimization to
     * avoid redundant calculations and drawing.
     */
    private boolean mUpdateBackgrounds;

    /** Constructor for the item decoration. */
    public SettingsItemBackgroundDecoration() {
        mUpdateBackgrounds = true;
    }

    /**
     * Updates the preference styles for the preferences.
     *
     * @param preferenceStyles The new list of preference styles.
     */
    public void updatePreferenceStyles(ArrayList<SettingsContainerStyle> preferenceStyles) {
        mPreferenceStyles = preferenceStyles;
        mUpdateBackgrounds = true;
    }

    @Override
    public void getItemOffsets(
            @NonNull Rect outRect,
            @NonNull View view,
            @NonNull RecyclerView parent,
            @NonNull RecyclerView.State state) {
        super.getItemOffsets(outRect, view, parent, state);
        mUpdateBackgrounds = true;

        int position = parent.getChildAdapterPosition(view);
        if (position == RecyclerView.NO_POSITION
                || mPreferenceStyles == null
                || position >= mPreferenceStyles.size()) {
            return;
        }

        SettingsContainerStyle style = mPreferenceStyles.get(position);
        outRect.top = style.getTopMargin();
        outRect.bottom = style.getBottomMargin();
        outRect.left = style.getHorizontalMargin();
        outRect.right = style.getHorizontalMargin();
    }

    @Override
    public void onDraw(
            @NonNull Canvas c, @NonNull RecyclerView parent, @NonNull RecyclerView.State state) {
        if (!mUpdateBackgrounds || mPreferenceStyles == null) return;

        for (int i = 0; i < parent.getChildCount(); i++) {
            View childView = parent.getChildAt(i);
            int position = parent.getChildAdapterPosition(childView);
            if (position == RecyclerView.NO_POSITION || position >= mPreferenceStyles.size()) {
                continue;
            }
            applyBackgroundStyle(childView, mPreferenceStyles.get(position));
        }
        mUpdateBackgrounds = false;
        super.onDraw(c, parent, state);
    }

    /**
     * Applies the specified background style to the given view.
     *
     * @param view The view to apply the background to.
     * @param style The {@link SettingsContainerStyle} to apply.
     */
    private void applyBackgroundStyle(View view, @NonNull SettingsContainerStyle style) {
        if (style == SettingsContainerStyle.EMPTY) {
            view.setBackground(null);
            return;
        }
        view.setBackground(
                createRoundedDrawable(
                        style.getTopRadius(), style.getBottomRadius(), style.getBackgroundColor()));
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
}
