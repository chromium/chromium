// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.settings.SettingsStylingController.BackgroundStyleDetails;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

import java.util.ArrayList;

/**
 * A {@link RecyclerView.ItemDecoration} that applies a specified custom background to the settings
 * items.
 */
@NullMarked
public class SettingsItemBackgroundDecoration extends RecyclerView.ItemDecoration {
    private final int mHorizontalMargin;
    private final int mVerticalMargin;
    private final Context mContext;
    private @Nullable ArrayList<BackgroundStyleDetails> mBackgroundStyleDetails;

    /**
     * A flag to ensure that the background update logic in {@link #onDraw(Canvas, RecyclerView,
     * RecyclerView.State)} is not executed on every frame. This is a performance optimization to
     * avoid redundant calculations and drawing.
     */
    private boolean mUpdateBackgrounds;

    /**
     * Constructor for the item decoration.
     *
     * @param context The context for accessing resources.
     */
    public SettingsItemBackgroundDecoration(Context context) {
        mContext = context;
        mUpdateBackgrounds = true;
        mBackgroundStyleDetails = null;
        mHorizontalMargin =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_horizontal_margin);
        mVerticalMargin =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_item_vertical_margin);
    }

    /**
     * Updates the background styles for the preferences.
     *
     * @param backgroundStyleDetails The new list of background styles.
     */
    public void updateBackgroundStyleDetails(
            ArrayList<BackgroundStyleDetails> backgroundStyleDetails) {
        mBackgroundStyleDetails = backgroundStyleDetails;
        mUpdateBackgrounds = true;
    }

    /** Returns the horizontal margin for the settings items. */
    public int getHorizontalMargin() {
        return mHorizontalMargin;
    }

    /** Returns the vertical margin for the settings items. */
    public int getVerticalMargin() {
        return mVerticalMargin;
    }

    @Override
    public void getItemOffsets(
            @NonNull Rect outRect,
            @NonNull View view,
            @NonNull RecyclerView parent,
            @NonNull RecyclerView.State state) {
        super.getItemOffsets(outRect, view, parent, state);
        mUpdateBackgrounds = true;
        outRect.top = getVerticalMargin();
        outRect.bottom = getVerticalMargin();
        outRect.left = getHorizontalMargin();
        outRect.right = getHorizontalMargin();
    }

    @Override
    public void onDraw(
            @NonNull Canvas c, @NonNull RecyclerView parent, @NonNull RecyclerView.State state) {
        if (!mUpdateBackgrounds || mBackgroundStyleDetails == null) return;

        for (int i = 0; i < parent.getChildCount(); i++) {
            View childView = parent.getChildAt(i);
            int position = parent.getChildAdapterPosition(childView);
            if (position == RecyclerView.NO_POSITION
                    || position >= mBackgroundStyleDetails.size()) {
                continue;
            }
            applyBackgroundStyle(childView, mBackgroundStyleDetails.get(position));
        }
        mUpdateBackgrounds = false;
        super.onDraw(c, parent, state);
    }

    /**
     * Applies the specified background style to the given view.
     *
     * @param view The view to apply the background to.
     * @param style The {@link BackgroundStyleDetails} to apply.
     */
    private void applyBackgroundStyle(View view, @NonNull BackgroundStyleDetails style) {
        if (style == BackgroundStyleDetails.EMPTY) {
            view.setBackground(null);
            return;
        }
        view.setBackground(createRoundedDrawable(mContext, style.topRadius, style.bottomRadius));
    }

    /**
     * Creates a rounded drawable with the specified top and bottom radii.
     *
     * @param context The context for accessing resources.
     * @param topRadius The radius for the top corners.
     * @param bottomRadius The radius for the bottom corners.
     * @return A new {@link Drawable} with the specified properties.
     */
    private static Drawable createRoundedDrawable(
            Context context, float topRadius, float bottomRadius) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setShape(GradientDrawable.RECTANGLE);
        drawable.setColor(SemanticColorUtils.getColorSurfaceContainerLowest(context));
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
