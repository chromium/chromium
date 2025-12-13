// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * A composite widget that displays a list of {@link RichRadioButton}s either in a single vertical
 * column or in a two-column grid layout, managing its own RecyclerView and Adapter.
 */
@NullMarked
public class RichRadioButtonList extends FrameLayout {

    /** The layout mode for the RichRadioButtonList. */
    @IntDef({LayoutMode.VERTICAL_SINGLE_COLUMN, LayoutMode.TWO_COLUMN_GRID})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LayoutMode {
        int VERTICAL_SINGLE_COLUMN = 0;
        int TWO_COLUMN_GRID = 1;
    }

    private @Nullable RichRadioButtonAdapter mAdapter;

    private @Nullable List<RichRadioButtonData> mCurrentOptions;
    private @LayoutMode int mCurrentLayoutMode;
    private final RecyclerView mRecyclerView;
    private boolean mInitialized;

    public RichRadioButtonList(@NonNull Context context) {
        this(context, null);
    }

    public RichRadioButtonList(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        inflate(context, R.layout.rich_radio_button_list, this);
        mRecyclerView = findViewById(R.id.rich_radio_button_list_recycler_view);
        mRecyclerView.setItemAnimator(null);
    }

    /**
     * Initializes the options to display and configures the list's layout. This method is intended
     * to be called only once after the component is created.
     *
     * @param options The list of RichRadioButtonData items to display.
     * @param layoutMode The desired layout mode (single column or two-column grid).
     */
    @Initializer
    public void initialize(
            @NonNull List<RichRadioButtonData> options,
            @LayoutMode int layoutMode,
            @NonNull RichRadioButtonAdapter.OnItemSelectedListener listener) {
        if (mInitialized) {
            throw new IllegalStateException("RichRadioButtonList can only be initialized once.");
        }
        mInitialized = true;

        mCurrentOptions = options;
        mCurrentLayoutMode = layoutMode;

        RecyclerView.LayoutManager layoutManager;

        if (layoutMode == LayoutMode.VERTICAL_SINGLE_COLUMN) {
            int verticalSpacingPx =
                    getContext()
                            .getResources()
                            .getDimensionPixelSize(R.dimen.rich_radio_button_list_vertical_spacing);
            layoutManager =
                    new LinearLayoutManager(getContext(), LinearLayoutManager.VERTICAL, false);
            clearItemDecorations();
            mRecyclerView.addItemDecoration(new SimpleItemDecoration(verticalSpacingPx, 0));
            mRecyclerView.getLayoutParams().width = ViewGroup.LayoutParams.MATCH_PARENT;
        } else {
            int horizontalSpacingPx =
                    getContext()
                            .getResources()
                            .getDimensionPixelSize(
                                    R.dimen.rich_radio_button_list_horizontal_spacing);

            layoutManager = new GridLayoutManager(getContext(), /* spanCount= */ 2);
            clearItemDecorations();
            mRecyclerView.addItemDecoration(
                    new SimpleItemDecoration(horizontalSpacingPx, horizontalSpacingPx));
            mRecyclerView.getLayoutParams().height = ViewGroup.LayoutParams.WRAP_CONTENT;
        }
        mRecyclerView.setLayoutManager(layoutManager);

        mAdapter = new RichRadioButtonAdapter(mCurrentOptions, listener, mCurrentLayoutMode);
        mRecyclerView.setAdapter(mAdapter);
    }

    /**
     * Sets the initially selected item.
     *
     * @param itemId The ID of the item to select.
     */
    public void setSelectedItem(@NonNull String itemId) {
        if (!mInitialized) {
            return;
        }
        if (mAdapter != null) {
            mAdapter.setSelectedItem(itemId);
        }
    }

    /** Clears all existing ItemDecorations from the RecyclerView. */
    private void clearItemDecorations() {
        List<RecyclerView.ItemDecoration> decorationsToRemove = new ArrayList<>();
        for (int i = 0; i < mRecyclerView.getItemDecorationCount(); i++) {
            decorationsToRemove.add(mRecyclerView.getItemDecorationAt(i));
        }

        for (RecyclerView.ItemDecoration decoration : decorationsToRemove) {
            mRecyclerView.removeItemDecoration(decoration);
        }
    }

    /**
     * ItemDecoration for spacing between items. GridLayouts have a built-in "space-between"
     * spacing.
     */
    private static class SimpleItemDecoration extends RecyclerView.ItemDecoration {
        private final int mVerticalSpaceHeightPx;
        private final int mHorizontalSpaceWidthPx;

        public SimpleItemDecoration(int verticalSpaceHeightPx, int horizontalSpaceWidthPx) {
            mVerticalSpaceHeightPx = verticalSpaceHeightPx;
            mHorizontalSpaceWidthPx = horizontalSpaceWidthPx;
        }

        @Override
        public void getItemOffsets(
                @NonNull Rect outRect,
                @NonNull View view,
                @NonNull RecyclerView parent,
                @NonNull RecyclerView.State state) {
            super.getItemOffsets(outRect, view, parent, state);

            int position = parent.getChildAdapterPosition(view);
            if (position == RecyclerView.NO_POSITION || parent.getAdapter() == null) return;

            int itemCount = parent.getAdapter().getItemCount();
            int spanCount = 1;
            if (parent.getLayoutManager() instanceof GridLayoutManager) {
                spanCount = ((GridLayoutManager) parent.getLayoutManager()).getSpanCount();
                int column = position % spanCount;
                outRect.left = column * mHorizontalSpaceWidthPx / spanCount;
                outRect.right =
                        mHorizontalSpaceWidthPx
                                - (column + 1) * mHorizontalSpaceWidthPx / spanCount;

                if (position < itemCount - spanCount) {
                    outRect.bottom = mVerticalSpaceHeightPx;
                }
                return;
            }

            if (position < itemCount - spanCount) {
                outRect.bottom = mVerticalSpaceHeightPx;
            }

            if ((position + 1) % spanCount != 0) {
                outRect.right = mHorizontalSpaceWidthPx;
            }
        }
    }

    RecyclerView getRecyclerViewForTesting() {
        return mRecyclerView;
    }

    @Nullable
    RichRadioButtonAdapter getAdapterForTesting() {
        return mAdapter;
    }
}
