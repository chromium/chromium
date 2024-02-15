// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.image_tiles;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.view.animation.LayoutAnimationController;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;
import androidx.recyclerview.widget.RecyclerView.State;

import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.animation.EmptyAnimationListener;
import org.chromium.ui.modelutil.ForwardingListObservable;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.RecyclerViewAdapter;

/**
 * The View component of the tiles UI.  This takes the {@link TileListModel} and creates the
 * glue to display it on the screen.
 */
class TileListView {
    private final TileListModel mModel;
    private final RecyclerView mView;
    private final RecyclerViewAdapter<TileViewHolder, Void> mAdapter;
    private final LinearLayoutManager mLayoutManager;
    private final LayoutAnimationController mLayoutAnimationController;
    private final TileSizeSupplier mTileSizeSupplier;

    /** Constructor. */
    public TileListView(Context context, TileConfig config, TileListModel model) {
        mModel = model;
        mView =
                new RecyclerView(context) {
                    @Override
                    protected void onConfigurationChanged(Configuration newConfig) {
                        super.onConfigurationChanged(newConfig);

                        // Reset the adapter to ensure that any cached views are recreated.
                        setAdapter(null);
                        setAdapter(mAdapter);
                        mTileSizeSupplier.recompute();
                    }
                };

        mView.setHasFixedSize(true);

        mLayoutManager = new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false);
        mView.setLayoutManager(mLayoutManager);
        mView.addItemDecoration(new ItemDecorationImpl(context));
        mView.setItemAnimator(null);
        mLayoutAnimationController =
                AnimationUtils.loadLayoutAnimation(context, R.anim.image_grid_enter);
        configureAnimationListener();
        mTileSizeSupplier = new TileSizeSupplier(context);

        PropertyModelChangeProcessor.create(
                mModel.getProperties(), mView, new TileListPropertyViewBinder());

        mAdapter =
                new RecyclerViewAdapter<>(
                        new ModelChangeProcessor(mModel),
                        new TileViewHolderFactory(mTileSizeSupplier));
        mView.setAdapter(mAdapter);
        mView.post(mAdapter::notifyDataSetChanged);
    }

    /** @return The Android {@link View} representing this widget. */
    public View getView() {
        return mView;
    }

    /** Scrolls to the beginning of the list if possible. */
    void scrollToBeginning() {
        if (mView.computeHorizontalScrollOffset() != 0) {
            mView.getLayoutManager().scrollToPosition(0);
        }
    }

    /** Called to show enter animation for the list items. */
    void showAnimation(boolean animate) {
        if (animate) {
            mView.setLayoutAnimation(mLayoutAnimationController);
            mView.scheduleLayoutAnimation();
        }
    }

    private void configureAnimationListener() {
        mView.setLayoutAnimationListener(
                new EmptyAnimationListener() {
                    @Override
                    public void onAnimationEnd(Animation animation) {
                        mView.setLayoutAnimation(null);
                    }
                });
    }

    private class ItemDecorationImpl extends ItemDecoration {
        private final int mInterCellPadding;

        public ItemDecorationImpl(Context context) {
            mInterCellPadding =
                    context.getResources()
                            .getDimensionPixelOffset(R.dimen.tile_grid_inter_tile_padding);
        }

        @Override
        public void getItemOffsets(
                @NonNull Rect outRect,
                @NonNull View view,
                @NonNull RecyclerView parent,
                @NonNull State state) {
            int position = parent.getChildAdapterPosition(view);
            if (position != 0) outRect.left = mInterCellPadding / 2;
            if (position != mModel.size() - 1) outRect.right = mInterCellPadding / 2;
        }
    }

    private static class ModelChangeProcessor extends ForwardingListObservable<Void>
            implements RecyclerViewAdapter.Delegate<TileViewHolder, Void> {
        private final TileListModel mModel;

        public ModelChangeProcessor(TileListModel model) {
            mModel = model;
            model.addObserver(this);
        }

        @Override
        public int getItemCount() {
            return mModel.size();
        }

        @Override
        public int getItemViewType(int position) {
            return 0;
        }

        @Override
        public void onBindViewHolder(
                TileViewHolder viewHolder, int position, @Nullable Void payload) {
            viewHolder.bind(mModel.getProperties(), mModel.get(position));
        }

        @Override
        public void onViewRecycled(TileViewHolder viewHolder) {
            viewHolder.recycle();
        }
    }
}
