// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments.ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.GradientDrawable;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.payments.R;

import java.util.Objects;

/** This is an item decorator for lists of items displayed in bottom sheets. */
@NullMarked
public class ItemDividerBase extends RecyclerView.ItemDecoration {
    private final Context mContext;

    /**
     * Creates an instance of ItemDividerBase.
     *
     * @param context Is used to get the drawable resources for the item backgrounds.
     */
    public ItemDividerBase(Context context) {
        mContext = context;
    }

    /**
     * Draws the decorations into the Canvas supplied to the RecyclerView.
     *
     * @param canvas The Canvas to draw into.
     * @param parent The RecyclerView this ItemDecoration is drawing into. The {@link
     *     RecyclerView#getAdapter()} must be present.
     * @param state The current state of RecyclerView.
     */
    @Override
    public void onDraw(
            @NonNull Canvas canvas, RecyclerView parent, @NonNull RecyclerView.State state) {
        assumeNonNull(parent.getAdapter());
        int itemCount = Objects.requireNonNull(parent.getAdapter()).getItemCount();
        for (int position = 0; position < parent.getChildCount(); position++) {
            loadBackgroundDrawable(parent.getChildAt(position), position, itemCount);
        }
    }

    /**
     * Sets the proper background for the drawable.
     *
     * @param view The view that the background is for.
     * @param position Indicates the position of the view in the list.
     * @param itemCount Shows how many items are in the list.
     */
    private void loadBackgroundDrawable(View view, int position, int itemCount) {
        GradientDrawable background =
                (GradientDrawable)
                        AppCompatResources.getDrawable(
                                mContext, selectBackgroundDrawable(position, itemCount));
        PaymentsUiUtil.addColorAndRippleToBackground(view, background, mContext);
    }

    /**
     * Returns the proper background for each of the items depending on their position.
     *
     * @param position Position of the item inside the list (including the header and the button).
     * @param itemCount Shows how many items are in the list (including the header and the button).
     * @return The ID of the selected background resource.
     */
    private int selectBackgroundDrawable(int position, int itemCount) {
        if (itemCount == 1) { // Round all the corners of the only item.
            return R.drawable.payments_item_background_rounded;
        }
        if (position == 0) { // Round the top of the first item.
            return R.drawable.payments_item_background_rounded_up;
        }
        if (position == itemCount - 1) { // Round the bottom of the last suggestion item.
            return R.drawable.payments_item_background_rounded_down;
        }

        // The rest of the items have a background with no rounded edges.
        return R.drawable.payments_item_background;
    }
}
