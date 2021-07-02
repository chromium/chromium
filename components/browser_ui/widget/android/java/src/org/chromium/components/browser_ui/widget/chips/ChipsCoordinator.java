// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.chips;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.Px;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;
import androidx.recyclerview.widget.RecyclerView.State;

import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

import java.util.List;

/**
 * The coordinator responsible for managing a list of chips.  To get the {@link View} that
 * represents this coordinator use {@link #getView()}.
 */
public class ChipsCoordinator implements ChipsProvider.Observer {
    private final ChipsProvider mProvider;
    private final ListModel<Chip> mModel = new ListModel<>();
    private final RecyclerView mView;

    /**
     * Builds and initializes this coordinator, including all sub-components.
     * @param context The {@link Context} to use to grab all of the resources.
     * @param provider The source for the underlying Chip state.
     */
    public ChipsCoordinator(Context context, ChipsProvider provider) {
        assert context != null;
        assert provider != null;

        mProvider = provider;

        // Build the underlying components.
        mView = createView(context, provider);

        mView.setAdapter(new RecyclerViewAdapter<>(
                new SimpleRecyclerViewMcp<>(mModel, null, ChipsViewHolder::bind),
                ChipsViewHolder::create));

        mProvider.addObserver(this);
        mModel.set(mProvider.getChips());
    }

    /**
     * Destroys the coordinator.  This should be called when the coordinator is no longer in use.
     * The coordinator should not be used after that point.
     */
    public void destroy() {
        mProvider.removeObserver(this);
    }

    /** @return The {@link View} that represents this coordinator. */
    public View getView() {
        return mView;
    }

    // ChipsProvider.Observer implementation.
    @Override
    public void onChipsChanged() {
        List<Chip> chips = mProvider.getChips();
        mModel.set(chips);
    }

    private static RecyclerView createView(Context context, ChipsProvider provider) {
        RecyclerView view = new RecyclerView(context);
        view.setLayoutManager(
                new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false));
        view.addItemDecoration(
                new SpaceItemDecoration(provider.getChipSpacingPx(), provider.getSidePaddingPx()));
        view.getItemAnimator().setChangeDuration(0);
        return view;
    }

    private static class SpaceItemDecoration extends ItemDecoration {
        private final int mChipSpacingPx;
        private final int mSidePaddingPx;

        public SpaceItemDecoration(@Px int chipSpacingPx, @Px int sidePaddingPx) {
            mChipSpacingPx = chipSpacingPx;
            mSidePaddingPx = sidePaddingPx;
        }

        @Override
        public void getItemOffsets(Rect outRect, View view, RecyclerView parent, State state) {
            int position = parent.getChildAdapterPosition(view);
            boolean isFirst = position == 0;
            boolean isLast = position == parent.getAdapter().getItemCount() - 1;

            outRect.left = isFirst ? mSidePaddingPx : mChipSpacingPx;
            outRect.right = isLast ? mSidePaddingPx : mChipSpacingPx;
        }
    }
}
