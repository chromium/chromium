// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.chips;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.Px;
import androidx.annotation.StyleRes;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemDecoration;
import androidx.recyclerview.widget.RecyclerView.State;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * The coordinator responsible for managing a list of chips. To get the {@link View} that represents
 * this coordinator use {@link #getView()}.
 */
public class ChipsCoordinator {
    private final ModelList mModelList;
    private final RecyclerView mView;

    /**
     * Builds and initializes this coordinator, including all sub-components.
     *
     * @param context The {@link Context} to use to grab all of the resources.
     * @param modelList The list of chip models to be displayed.
     */
    public ChipsCoordinator(Context context, ModelList modelList) {
        this(context, modelList, R.style.SuggestionChipThemeOverlay);
    }

    public ChipsCoordinator(Context context, ModelList modelList, @StyleRes int themeOverlay) {
        assert context != null;
        assert modelList != null;

        mModelList = modelList;

        // Build the underlying components.
        mView = new RecyclerView(context);
        mView.setLayoutManager(
                new LinearLayoutManager(context, LinearLayoutManager.HORIZONTAL, false));
        mView.getItemAnimator().setChangeDuration(0);

        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(mModelList);
        adapter.registerType(
                ChipProperties.BASIC_CHIP,
                (parent) -> new ChipView(context, themeOverlay),
                ChipViewBinder::bind);
        mView.setAdapter(adapter);
    }

    /**
     * A utility method for more easily creating basic chip model. This model can be altered after
     * construction.
     *
     * @param id The ID of the chip.
     * @param text The text to display in the chip. The {@link ChipProperties#CONTENT_DESCRIPTION}
     *     also uses this value by default.
     * @param clickHandler A handler for when the chip is selected.
     * @param iconId An icon ID to show beside the chip's text. If not specified, this will be
     *     {@link ChipProperties#ICON}.
     * @return A list item containing a property model for a basic chip.
     */
    public static ListItem buildChipListItem(
            int id, String text, Callback<PropertyModel> clickHandler, @DrawableRes int iconId) {
        PropertyModel model =
                new PropertyModel.Builder(ChipProperties.ALL_KEYS)
                        .with(ChipProperties.ID, id)
                        .with(ChipProperties.TEXT, text)
                        .with(ChipProperties.CONTENT_DESCRIPTION, text)
                        .with(ChipProperties.CLICK_HANDLER, clickHandler)
                        .with(ChipProperties.ICON, iconId)
                        .with(ChipProperties.APPLY_ICON_TINT, true)
                        .with(ChipProperties.ENABLED, true)
                        .with(ChipProperties.SELECTED, false)
                        .with(ChipProperties.TEXT_MAX_WIDTH_PX, ChipProperties.SHOW_WHOLE_TEXT)
                        .build();
        return new ListItem(ChipProperties.BASIC_CHIP, model);
    }

    /** @see {@link #buildChipListItem(int, String, Callback, int)} */
    public static ListItem buildChipListItem(
            int id, String text, Callback<PropertyModel> clickHandler) {
        return buildChipListItem(id, text, clickHandler, ChipProperties.INVALID_ICON_ID);
    }

    /**
     * Destroys the coordinator. This should be called when the coordinator is no longer in use. The
     * coordinator should not be used after that point.
     */
    public void destroy() {}

    /** @return The {@link View} that represents this coordinator. */
    public View getView() {
        return mView;
    }

    /**
     * Set the spacing and padding between each chip.
     *
     * @param chipSpacingPx The spacing between each chip.
     * @param sidePaddingPx The side padding at the start and end of the list.
     */
    public void setSpaceItemDecoration(@Px int chipSpacingPx, @Px int sidePaddingPx) {
        mView.addItemDecoration(new SpaceItemDecoration(chipSpacingPx, sidePaddingPx));
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
            // using 'parent' not 'view' since 'view' did not layout yet, so view's
            // #getLayoutDirection() won't return correct value.
            boolean isRtl = (parent.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL);

            @Px int startPadding = isFirst ? mSidePaddingPx : mChipSpacingPx;
            @Px int endPadding = isLast ? mSidePaddingPx : mChipSpacingPx;

            outRect.left = isRtl ? endPadding : startPadding;
            outRect.right = isRtl ? startPadding : endPadding;
        }
    }
}
