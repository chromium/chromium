// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.containment;

import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

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

    /** Returns the {@link ContainmentItemController} used by this decoration. */
    public ContainmentItemController getStylingController() {
        return mStylingController;
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
            Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
        // Zero outRect directly instead of calling super.getItemOffsets(). The default
        // implementation in RecyclerView.ItemDecoration casts view.getLayoutParams() to
        // RecyclerView.LayoutParams, which can throw ClassCastException if the view has generic
        // LayoutParams (e.g. in unit tests or before attachment), so just zero it directly.
        // Furthermore, item containment spacing is applied as child view margins in
        // ContainmentViewStyler rather than decoration offsets.
        outRect.set(0, 0, 0, 0);
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
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        if (!mUpdateBackgrounds || mPreferenceStyles == null) return;

        int childCount = parent.getChildCount();
        for (int i = 0; i < childCount; i++) {
            View childView = parent.getChildAt(i);
            int position = parent.getChildAdapterPosition(childView);
            if (position == RecyclerView.NO_POSITION || position >= mPreferenceStyles.size()) {
                continue;
            }
            ContainmentViewStyler.applyPadding(childView, mPreferenceStyles.get(position));
            ContainmentViewStyler.applyBackgroundStyle(childView, mPreferenceStyles.get(position));
            ContainmentViewStyler.styleChildViews(childView, mStylingController);
        }

        // Clear the update flag once visible attached children have been styled.
        // Subsequent layout passes (e.g. when new views scroll into view or are attached)
        // will invoke getItemOffsets() which resets mUpdateBackgrounds to true.
        mUpdateBackgrounds = false;
        super.onDraw(c, parent, state);
    }

    /** Returns the {@link ContainerStyle} object for a given position. */
    public @Nullable ContainerStyle getContainerStyle(int position) {
        return (mPreferenceStyles != null && position >= 0 && position < mPreferenceStyles.size())
                ? mPreferenceStyles.get(position)
                : null;
    }

    boolean getUpdateBackgroundsForTesting() {
        return mUpdateBackgrounds;
    }
}
