// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.bottomsheet;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.browser_ui.widget.R.drawable.card_with_corners_background;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.util.TypedValue;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.Adapter;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** This is an item decorator for lists of items displayed in BottomSheets. */
@NullMarked
public abstract class ItemDividerBase extends RecyclerView.ItemDecoration {
    protected final Context mContext;

    protected @DrawableRes int getRoundedAllDrawableId() {
        return card_with_corners_background;
    }

    protected @DrawableRes int getRoundedUpDrawableId() {
        return R.drawable.list_item_background_top;
    }

    protected @DrawableRes int getRoundedDownDrawableId() {
        return R.drawable.list_item_background_bottom;
    }

    protected @DrawableRes int getMiddleDrawableId() {
        return R.drawable.list_item_background_middle;
    }

    protected @ColorInt int getBackgroundTintColor() {
        return SemanticColorUtils.getColorSurfaceContainerLow(mContext);
    }

    private void loadBackgroundDrawable(@Nullable View view, @DrawableRes int backgroundId) {
        if (view == null) {
            // RecyclerView might return a {@code null} view if it's not displayed to the user.
            return;
        }
        GradientDrawable background =
                (GradientDrawable) AppCompatResources.getDrawable(mContext, backgroundId);
        TypedValue themeRes = new TypedValue();
        mContext.getTheme().resolveAttribute(android.R.attr.colorControlHighlight, themeRes, true);
        RippleDrawable rippleDrawable =
                new RippleDrawable(ColorStateList.valueOf(themeRes.data), background, null);
        view.setBackground(rippleDrawable);
        view.setBackgroundTintList(ColorStateList.valueOf(getBackgroundTintColor()));
    }

    private @DrawableRes int getBackgroundDrawableId(
            boolean isFirstInSection, boolean isLastInSection) {
        if (isFirstInSection && isLastInSection) {
            return getRoundedAllDrawableId();
        }
        if (isFirstInSection) {
            return getRoundedUpDrawableId();
        }
        if (isLastInSection) {
            return getRoundedDownDrawableId();
        }
        return getMiddleDrawableId();
    }

    /**
     * Draws the decorations into the Canvas supplied to the RecyclerView.
     *
     * @param canvas The Canvas to draw into.
     * @param parent The RecyclerView this ItemDecoration is drawing into.
     * @param state The current state of RecyclerView.
     */
    @Override
    public void onDraw(Canvas canvas, RecyclerView parent, RecyclerView.State state) {
        Adapter<?> adapter = assumeNonNull(parent.getAdapter());
        int itemCount = adapter.getItemCount();
        for (int childIndex = 0; childIndex < parent.getChildCount(); childIndex++) {
            View child = parent.getChildAt(childIndex);
            int adapterPosition = parent.getChildAdapterPosition(child);
            if (adapterPosition == RecyclerView.NO_POSITION
                    || adapterPosition >= itemCount
                    || shouldSkipItemType(adapter.getItemViewType(adapterPosition))) {
                continue;
            }
            boolean isFirstInSection =
                    adapterPosition == 0
                            || shouldSkipItemType(adapter.getItemViewType(adapterPosition - 1));
            boolean isLastInSection =
                    adapterPosition == itemCount - 1
                            || shouldSkipItemType(adapter.getItemViewType(adapterPosition + 1));
            loadBackgroundDrawable(
                    child, getBackgroundDrawableId(isFirstInSection, isLastInSection));
        }
    }

    /**
     * Creates an instance of ItemDividerBase.
     *
     * @param context Is used to get the drawable resources for the item backgrounds.
     */
    protected ItemDividerBase(Context context) {
        this.mContext = context;
    }

    /**
     * Used as helper to determine undecorated items like headers and buttons.
     *
     * @param type A type of an item in the list on the {@link BottomSheet}.
     * @return True if the item of the said type should be undecorated.
     */
    protected abstract boolean shouldSkipItemType(int type);
}
