// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.containment;

import android.graphics.Canvas;
import android.graphics.Rect;
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
public class ContainmentItemDecoration extends RecyclerView.ItemDecoration {
    private final ContainmentItemController mStylingController;
    private @Nullable ArrayList<ContainerStyle> mPreferenceStyles;

    /**
     * A flag to ensure that the background update logic in {@link #onDraw(Canvas, RecyclerView,
     * RecyclerView.State)} is not executed on every frame. This is a performance optimization to
     * avoid redundant calculations and drawing.
     */
    private boolean mUpdateBackgrounds;

    /**
     * Constructor for the item decoration.
     *
     * @param stylingController The {@link ContainmentItemController} for styling the child views.
     */
    public ContainmentItemDecoration(ContainmentItemController stylingController) {
        mStylingController = stylingController;
        mUpdateBackgrounds = true;
    }

    /**
     * Updates the preference styles for the preferences.
     *
     * @param preferenceStyles The new list of preference styles.
     */
    public void updatePreferenceStyles(ArrayList<ContainerStyle> preferenceStyles) {
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

        ContainmentViewStyler.applyMargins(view, mPreferenceStyles.get(position));
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
            ContainmentViewStyler.applyPadding(childView, mPreferenceStyles.get(position));
            ContainmentViewStyler.applyBackgroundStyle(childView, mPreferenceStyles.get(position));
            ContainmentViewStyler.styleChildViews(childView, mStylingController);
        }
        mUpdateBackgrounds = false;
        super.onDraw(c, parent, state);
    }

    /** Returns the {@link ContainerStyle} object for a given position. */
    public @Nullable ContainerStyle getContainerStyle(int position) {
        return mPreferenceStyles != null ? mPreferenceStyles.get(position) : null;
    }
}
